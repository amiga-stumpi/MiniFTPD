#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

#include "miniftpd/config.h"
#include "miniftpd/listing.h"
#include "miniftpd/path.h"
#include "miniftpd/server.h"
#include "miniftpd/session.h"
#include "miniftpd/transfer.h"
#include "miniftpd/socket_api.h"

#define CONTROL_RECV_SIZE 256
#define SEND_TIMEOUT_SECONDS 5
#define DATA_CONNECT_TIMEOUT_SECONDS 10
#define DATA_TRANSFER_TIMEOUT_SECONDS 10

struct MiniFtpdServer
{
    struct Library *socket_base;
    const struct MiniFtpdConfig *config;
    int listen_fd;
    struct MiniFtpdSession session;
    UBYTE command_overflow;
    UBYTE stop_requested;
    UWORD next_pasv_port;
};

static UBYTE g_control_recv[CONTROL_RECV_SIZE];
static char g_path_reply[MINIFTPD_CWD_SIZE + 48];
static char g_normalized_path[MINIFTPD_CWD_SIZE];
static char g_resolved_path[MINIFTPD_PATH_SIZE * 2];
static struct MiniFtpdFdSet g_readfds;
static struct MiniFtpdFdSet g_writefds;
static struct MiniFtpdTimeVal g_timeout;

static void accept_data_client(struct MiniFtpdServer *server);

static void console_write(const char *text)
{
    BPTR output;

    output = Output();
    if (output && text)
        Write(output, (APTR)text, (LONG)strlen(text));
}

static void console_number(const char *label, LONG value)
{
    char line[96];

    sprintf(line, "%s%d\n", label, (int)value);
    console_write(line);
}

static int ctrl_c_pending(void)
{
    ULONG pending;

    pending = SetSignal(0, 0);
    if (!(pending & SIGBREAKF_CTRL_C))
        return 0;
    SetSignal(0, SIGBREAKF_CTRL_C);
    return 1;
}

static int socket_would_block(int error)
{
    return error == MINIFTPD_EWOULDBLOCK || error == MINIFTPD_EAGAIN_OLD;
}

static void reset_session(struct MiniFtpdSession *session)
{
    memset(session, 0, sizeof(*session));
    session->control_fd = -1;
    session->passive_listen_fd = -1;
    session->data_fd = -1;
    session->connection_state = MINIFTPD_SESSION_DISCONNECTED;
    session->transfer_mode = MINIFTPD_TRANSFER_BINARY;
    session->cwd[0] = '/';
    session->cwd[1] = '\0';
}

static void close_session_socket(struct MiniFtpdServer *server, int *fd)
{
    if (*fd >= 0) {
        miniftpd_close_socket(server->socket_base, *fd);
        *fd = -1;
    }
}

static int set_nonblocking(struct MiniFtpdServer *server, int fd)
{
    ULONG enabled;

    enabled = 1;
    return miniftpd_ioctl(server->socket_base, fd, MINIFTPD_FIONBIO,
                          &enabled) >= 0;
}

static void close_client(struct MiniFtpdServer *server)
{
    close_session_socket(server, &server->session.data_fd);
    close_session_socket(server, &server->session.passive_listen_fd);
    close_session_socket(server, &server->session.control_fd);
    reset_session(&server->session);
    server->command_overflow = 0;
    console_write("FTP control client disconnected.\n");
}

static int wait_writable(struct MiniFtpdServer *server, int fd)
{
    ULONG signals;
    int ready;

    MINIFTPD_FD_ZERO(&g_writefds);
    MINIFTPD_FD_SET(fd, &g_writefds);
    g_timeout.tv_sec = SEND_TIMEOUT_SECONDS;
    g_timeout.tv_usec = 0;
    signals = SIGBREAKF_CTRL_C;
    ready = miniftpd_wait_select(server->socket_base, fd + 1, 0,
                                 &g_writefds, 0, &g_timeout, &signals);
    if ((signals & SIGBREAKF_CTRL_C) || ctrl_c_pending()) {
        server->stop_requested = 1;
        return -1;
    }
    if (ready <= 0)
        return 0;
    return MINIFTPD_FD_ISSET(fd, &g_writefds) ? 1 : 0;
}

static int send_all(struct MiniFtpdServer *server, int fd,
                    const char *text, int length)
{
    int sent;
    int total;
    int error;
    int ready;

    total = 0;
    while (total < length) {
        sent = miniftpd_send(server->socket_base, fd, text + total,
                             length - total, 0);
        if (sent > 0) {
            total += sent;
            continue;
        }
        error = miniftpd_socket_errno(server->socket_base);
        if (!socket_would_block(error))
            return 0;
        ready = wait_writable(server, fd);
        if (ready <= 0)
            return 0;
    }
    return 1;
}

static int send_reply(struct MiniFtpdServer *server, int fd,
                      const char *reply)
{
    return send_all(server, fd, reply, strlen(reply));
}

static char upper_ascii(char value)
{
    if (value >= 'a' && value <= 'z')
        return (char)(value - ('a' - 'A'));
    return value;
}

static int text_equal_ci(const char *left, const char *right)
{
    while (*left && *right) {
        if (upper_ascii(*left) != upper_ascii(*right))
            return 0;
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static int command_is(const char *line, const char *command)
{
    int position;

    position = 0;
    while (command[position]) {
        if (upper_ascii(line[position]) != command[position])
            return 0;
        ++position;
    }
    return line[position] == '\0' || line[position] == ' ' ||
           line[position] == '\t';
}

static const char *command_argument(const char *line)
{
    while (*line && *line != ' ' && *line != '\t')
        ++line;
    while (*line == ' ' || *line == '\t')
        ++line;
    return line;
}

static const char *list_path_argument(const char *line)
{
    const char *argument;

    argument = command_argument(line);
    while (argument[0] == '-') {
        while (*argument && *argument != ' ' && *argument != '\t')
            ++argument;
        while (*argument == ' ' || *argument == '\t')
            ++argument;
    }
    return argument;
}

static int wait_for_data_connection(struct MiniFtpdServer *server)
{
    ULONG signals;
    int ready;

    if (server->session.data_fd >= 0)
        return 1;
    if (server->session.passive_listen_fd < 0)
        return 0;
    MINIFTPD_FD_ZERO(&g_readfds);
    MINIFTPD_FD_SET(server->session.passive_listen_fd, &g_readfds);
    g_timeout.tv_sec = DATA_CONNECT_TIMEOUT_SECONDS;
    g_timeout.tv_usec = 0;
    signals = SIGBREAKF_CTRL_C;
    ready = miniftpd_wait_select(server->socket_base,
                                 server->session.passive_listen_fd + 1,
                                 &g_readfds, 0, 0, &g_timeout, &signals);
    if ((signals & SIGBREAKF_CTRL_C) || ctrl_c_pending()) {
        server->stop_requested = 1;
        return 0;
    }
    if (ready <= 0 ||
        !MINIFTPD_FD_ISSET(server->session.passive_listen_fd, &g_readfds))
        return 0;
    accept_data_client(server);
    return server->session.data_fd >= 0;
}

static int listing_write(void *context, const char *data, int length)
{
    struct MiniFtpdServer *server = (struct MiniFtpdServer *)context;

    return send_all(server, server->session.data_fd, data, length);
}

static int retrieve_write(void *context, const UBYTE *data, int length)
{
    struct MiniFtpdServer *server = (struct MiniFtpdServer *)context;

    return send_all(server, server->session.data_fd,
                    (const char *)data, length);
}

static int store_read(void *context, UBYTE *data, int length)
{
    struct MiniFtpdServer *server = (struct MiniFtpdServer *)context;
    ULONG signals;
    int received;
    int error;
    int ready;

    for (;;) {
        received = miniftpd_recv(server->socket_base,
                                 server->session.data_fd,
                                 data, length, 0);
        if (received >= 0)
            return received;
        error = miniftpd_socket_errno(server->socket_base);
        if (!socket_would_block(error))
            return -1;
        MINIFTPD_FD_ZERO(&g_readfds);
        MINIFTPD_FD_SET(server->session.data_fd, &g_readfds);
        g_timeout.tv_sec = DATA_TRANSFER_TIMEOUT_SECONDS;
        g_timeout.tv_usec = 0;
        signals = SIGBREAKF_CTRL_C;
        ready = miniftpd_wait_select(server->socket_base,
                                     server->session.data_fd + 1,
                                     &g_readfds, 0, 0,
                                     &g_timeout, &signals);
        if ((signals & SIGBREAKF_CTRL_C) || ctrl_c_pending()) {
            server->stop_requested = 1;
            return -1;
        }
        if (ready <= 0 ||
            !MINIFTPD_FD_ISSET(server->session.data_fd, &g_readfds))
            return -1;
    }
}

static int receive_file(struct MiniFtpdServer *server, const char *line)
{
    const char *argument;
    LONG file_size;
    int result;

    argument = command_argument(line);
    if (!argument[0] ||
        !miniftpd_path_normalize(server->session.cwd, argument,
                                 g_normalized_path,
                                 sizeof(g_normalized_path)) ||
        !miniftpd_path_resolve_upload(server->config->root,
                                      g_normalized_path,
                                      g_resolved_path,
                                      sizeof(g_resolved_path)))
        return send_reply(server, server->session.control_fd,
                          "550 File unavailable.\r\n");
    if (!wait_for_data_connection(server)) {
        close_session_socket(server, &server->session.data_fd);
        close_session_socket(server, &server->session.passive_listen_fd);
        return send_reply(server, server->session.control_fd,
                          "425 Use PASV first.\r\n");
    }
    if (!send_reply(server, server->session.control_fd,
                    "150 Opening data connection for file upload.\r\n"))
        return 0;
    result = miniftpd_store_file(server->config->root,
                                 g_normalized_path,
                                 store_read, server, &file_size);
    close_session_socket(server, &server->session.data_fd);
    close_session_socket(server, &server->session.passive_listen_fd);
    if (result == MINIFTPD_STOR_OK) {
        sprintf(g_path_reply,
                "226 Transfer complete (%ld bytes received).\r\n",
                (long)file_size);
        return send_reply(server, server->session.control_fd, g_path_reply);
    }
    if (result == MINIFTPD_STOR_READ_ERROR)
        return send_reply(server, server->session.control_fd,
                          "426 Data connection error; partial file kept.\r\n");
    if (result == MINIFTPD_STOR_NO_MEMORY)
        return send_reply(server, server->session.control_fd,
                          "451 Not enough memory for transfer.\r\n");
    if (result == MINIFTPD_STOR_WRITE_ERROR)
        return send_reply(server, server->session.control_fd,
                          "452 Could not write file; partial file kept.\r\n");
    return send_reply(server, server->session.control_fd,
                      "550 File unavailable.\r\n");
}

static int send_file(struct MiniFtpdServer *server, const char *line)
{
    const char *argument;
    LONG file_size;
    int result;

    argument = command_argument(line);
    if (!argument[0] ||
        !miniftpd_path_normalize(server->session.cwd, argument,
                                 g_normalized_path,
                                 sizeof(g_normalized_path)) ||
        !miniftpd_path_resolve_file(server->config->root,
                                    g_normalized_path,
                                    g_resolved_path,
                                    sizeof(g_resolved_path),
                                    &file_size))
        return send_reply(server, server->session.control_fd,
                          "550 File unavailable.\r\n");
    if (!wait_for_data_connection(server)) {
        close_session_socket(server, &server->session.data_fd);
        close_session_socket(server, &server->session.passive_listen_fd);
        return send_reply(server, server->session.control_fd,
                          "425 Use PASV first.\r\n");
    }
    sprintf(g_path_reply,
            "150 Opening data connection for file transfer (%ld bytes).\r\n",
            (long)file_size);
    if (!send_reply(server, server->session.control_fd, g_path_reply))
        return 0;
    result = miniftpd_retrieve_file(server->config->root,
                                    g_normalized_path,
                                    retrieve_write, server, 0);
    close_session_socket(server, &server->session.data_fd);
    close_session_socket(server, &server->session.passive_listen_fd);
    if (result == MINIFTPD_RETR_OK)
        return send_reply(server, server->session.control_fd,
                          "226 Transfer complete.\r\n");
    if (result == MINIFTPD_RETR_WRITE_ERROR)
        return send_reply(server, server->session.control_fd,
                          "426 Data connection error.\r\n");
    if (result == MINIFTPD_RETR_NO_MEMORY)
        return send_reply(server, server->session.control_fd,
                          "451 Not enough memory for transfer.\r\n");
    return send_reply(server, server->session.control_fd,
                      "451 Could not read file.\r\n");
}

static int send_directory_listing(struct MiniFtpdServer *server,
                                  const char *line)
{
    const char *argument;
    int result;

    argument = list_path_argument(line);
    if (argument[0]) {
        if (!miniftpd_path_normalize(server->session.cwd, argument,
                                     g_normalized_path,
                                     sizeof(g_normalized_path)))
            return send_reply(server, server->session.control_fd,
                              "550 Directory unavailable.\r\n");
    } else {
        strcpy(g_normalized_path, server->session.cwd);
    }
    if (!miniftpd_path_directory_exists(server->config->root,
                                        g_normalized_path))
        return send_reply(server, server->session.control_fd,
                          "550 Directory unavailable.\r\n");
    if (!wait_for_data_connection(server)) {
        close_session_socket(server, &server->session.data_fd);
        close_session_socket(server, &server->session.passive_listen_fd);
        return send_reply(server, server->session.control_fd,
                          "425 Use PASV first.\r\n");
    }
    if (!send_reply(server, server->session.control_fd,
                    "150 Opening ASCII mode data connection for file list.\r\n"))
        return 0;
    result = miniftpd_list_directory(server->config->root,
                                     g_normalized_path,
                                     listing_write, server);
    close_session_socket(server, &server->session.data_fd);
    close_session_socket(server, &server->session.passive_listen_fd);
    if (result > 0)
        return send_reply(server, server->session.control_fd,
                          "226 Directory send OK.\r\n");
    if (result < 0)
        return send_reply(server, server->session.control_fd,
                          "426 Data connection error.\r\n");
    return send_reply(server, server->session.control_fd,
                      "451 Could not read directory.\r\n");
}

static int open_passive_listener(struct MiniFtpdServer *server)
{
    struct MiniFtpdSockAddrIn local_address;
    struct MiniFtpdSockAddrIn passive_address;
    ULONG ip;
    UWORD port;
    ULONG attempts;
    ULONG range;
    int address_length;
    int fd;
    char reply[96];

    close_session_socket(server, &server->session.data_fd);
    close_session_socket(server, &server->session.passive_listen_fd);
    address_length = sizeof(local_address);
    if (miniftpd_getsockname(server->socket_base, server->session.control_fd,
                             (struct MiniFtpdSockAddr *)&local_address,
                             &address_length) < 0)
        return send_reply(server, server->session.control_fd,
                          "425 Cannot determine local address.\r\n");
    ip = local_address.sin_addr.s_addr;
    if (!ip)
        return send_reply(server, server->session.control_fd,
                          "425 Invalid local address.\r\n");
    range = (ULONG)server->config->pasv_port_max -
            (ULONG)server->config->pasv_port_min + 1UL;
    for (attempts = 0; attempts < range; ++attempts) {
        port = server->next_pasv_port;
        ++server->next_pasv_port;
        if (server->next_pasv_port > server->config->pasv_port_max)
            server->next_pasv_port = server->config->pasv_port_min;
        fd = miniftpd_socket(server->socket_base, MINIFTPD_AF_INET,
                             MINIFTPD_SOCK_STREAM, MINIFTPD_IPPROTO_TCP);
        if (fd < 0)
            break;
        memset(&passive_address, 0, sizeof(passive_address));
        passive_address.sin_len = sizeof(passive_address);
        passive_address.sin_family = MINIFTPD_AF_INET;
        passive_address.sin_port = port;
        passive_address.sin_addr.s_addr = ip;
        if (miniftpd_bind(server->socket_base, fd,
                          (const struct MiniFtpdSockAddr *)&passive_address,
                          sizeof(passive_address)) >= 0 &&
            miniftpd_listen(server->socket_base, fd, 1) >= 0 &&
            set_nonblocking(server, fd)) {
            server->session.passive_listen_fd = fd;
            sprintf(reply,
                    "227 Entering Passive Mode (%lu,%lu,%lu,%lu,%u,%u).\r\n",
                    (unsigned long)((ip >> 24) & 255UL),
                    (unsigned long)((ip >> 16) & 255UL),
                    (unsigned long)((ip >> 8) & 255UL),
                    (unsigned long)(ip & 255UL),
                    (unsigned)(port >> 8), (unsigned)(port & 255));
            return send_reply(server, server->session.control_fd, reply);
        }
        miniftpd_close_socket(server->socket_base, fd);
    }
    return send_reply(server, server->session.control_fd,
                      "425 No passive ports available.\r\n");
}

static int process_command(struct MiniFtpdServer *server)
{
    char *line;

    line = server->session.command;
    while (*line == ' ' || *line == '\t')
        ++line;
    if (!line[0])
        return 1;
    if (command_is(line, "PASS"))
        console_write("FTP command: PASS <hidden>\n");
    else {
        console_write("FTP command: ");
        console_write(line);
        console_write("\n");
    }
    if (command_is(line, "QUIT")) {
        send_reply(server, server->session.control_fd,
                   "221 MiniFTPD closing connection.\r\n");
        return 0;
    }
    if (command_is(line, "SYST"))
        return send_reply(server, server->session.control_fd,
                          "215 AMIGA Type: L8.\r\n");
    if (command_is(line, "FEAT"))
        return send_reply(server, server->session.control_fd,
                          "211 No additional features.\r\n");
    if (command_is(line, "NOOP"))
        return send_reply(server, server->session.control_fd,
                          "200 NOOP command successful.\r\n");
    if (command_is(line, "USER")) {
        const char *argument = command_argument(line);
        server->session.login_state = MINIFTPD_LOGIN_USER;
        server->session.anonymous_login = 0;
        server->session.user_valid = 0;
        if (argument[0] && !strcmp(argument, server->config->user))
            server->session.user_valid = 1;
        else if (server->config->anonymous &&
                 (text_equal_ci(argument, "anonymous") ||
                  text_equal_ci(argument, "ftp"))) {
            server->session.user_valid = 1;
            server->session.anonymous_login = 1;
        }
        return send_reply(server, server->session.control_fd,
                          "331 Password required.\r\n");
    }
    if (command_is(line, "PASS")) {
        const char *argument = command_argument(line);
        int authenticated;

        if (server->session.login_state != MINIFTPD_LOGIN_USER)
            return send_reply(server, server->session.control_fd,
                              "503 Login with USER first.\r\n");
        authenticated = server->session.user_valid &&
            (server->session.anonymous_login ||
             !strcmp(argument, server->config->password));
        if (authenticated) {
            server->session.login_state = MINIFTPD_LOGIN_AUTHENTICATED;
            server->session.failed_logins = 0;
            return send_reply(server, server->session.control_fd,
                              "230 Login successful.\r\n");
        }
        server->session.login_state = MINIFTPD_LOGIN_NONE;
        server->session.user_valid = 0;
        server->session.anonymous_login = 0;
        if (server->session.failed_logins < 255)
            ++server->session.failed_logins;
        if (server->session.failed_logins >= 3) {
            send_reply(server, server->session.control_fd,
                       "421 Too many login failures.\r\n");
            return 0;
        }
        return send_reply(server, server->session.control_fd,
                          "530 Login incorrect.\r\n");
    }
    if (server->session.login_state != MINIFTPD_LOGIN_AUTHENTICATED)
        return send_reply(server, server->session.control_fd,
                          "530 Please login with USER and PASS.\r\n");
    if (command_is(line, "PWD")) {
        sprintf(g_path_reply, "257 \"%s\" is the current directory.\r\n",
                server->session.cwd);
        return send_reply(server, server->session.control_fd, g_path_reply);
    }
    if (command_is(line, "CWD")) {
        const char *argument = command_argument(line);

        if (miniftpd_path_normalize(server->session.cwd, argument,
                                    g_normalized_path, sizeof(g_normalized_path)) &&
            miniftpd_path_directory_exists(server->config->root,
                                           g_normalized_path)) {
            strcpy(server->session.cwd, g_normalized_path);
            return send_reply(server, server->session.control_fd,
                              "250 Directory changed.\r\n");
        }
        return send_reply(server, server->session.control_fd,
                          "550 Directory unavailable.\r\n");
    }
    if (command_is(line, "CDUP")) {
        if (!strcmp(server->session.cwd, "/"))
            return send_reply(server, server->session.control_fd,
                              "250 Directory changed.\r\n");
        if (miniftpd_path_normalize(server->session.cwd, "..",
                                    g_normalized_path, sizeof(g_normalized_path)) &&
            miniftpd_path_directory_exists(server->config->root,
                                           g_normalized_path)) {
            strcpy(server->session.cwd, g_normalized_path);
            return send_reply(server, server->session.control_fd,
                              "250 Directory changed.\r\n");
        }
        return send_reply(server, server->session.control_fd,
                          "550 Directory unavailable.\r\n");
    }
    if (command_is(line, "STOR") &&
        !miniftpd_path_writes_allowed(server->config))
        return send_reply(server, server->session.control_fd,
                          "550 Server is read-only.\r\n");
    if (command_is(line, "STOR"))
        return receive_file(server, line);
    if (command_is(line, "RETR"))
        return send_file(server, line);
    if (command_is(line, "LIST"))
        return send_directory_listing(server, line);
    if (command_is(line, "PASV"))
        return open_passive_listener(server);
    if (command_is(line, "TYPE")) {
        const char *argument = command_argument(line);
        if ((argument[0] == 'A' || argument[0] == 'a') && !argument[1]) {
            server->session.transfer_mode = MINIFTPD_TRANSFER_ASCII;
            return send_reply(server, server->session.control_fd,
                              "200 Type set to A.\r\n");
        }
        if ((argument[0] == 'I' || argument[0] == 'i') && !argument[1]) {
            server->session.transfer_mode = MINIFTPD_TRANSFER_BINARY;
            return send_reply(server, server->session.control_fd,
                              "200 Type set to I.\r\n");
        }
        return send_reply(server, server->session.control_fd,
                          "504 Unsupported TYPE.\r\n");
    }
    return send_reply(server, server->session.control_fd,
                      "502 Command not implemented.\r\n");
}

static int consume_control_data(struct MiniFtpdServer *server,
                                const UBYTE *data, int length)
{
    int position;
    UBYTE value;

    for (position = 0; position < length; ++position) {
        value = data[position];
        if (value == '\r')
            continue;
        if (value == '\n') {
            if (server->command_overflow) {
                if (!send_reply(server, server->session.control_fd,
                                "500 Command line too long.\r\n"))
                    return 0;
            } else {
                server->session.command[server->session.command_len] = '\0';
                if (!process_command(server))
                    return 0;
            }
            server->session.command_len = 0;
            server->command_overflow = 0;
            continue;
        }
        if (server->command_overflow)
            continue;
        if (server->session.command_len >= MINIFTPD_COMMAND_SIZE - 1) {
            server->command_overflow = 1;
            continue;
        }
        server->session.command[server->session.command_len++] = (char)value;
    }
    return 1;
}

static int receive_control(struct MiniFtpdServer *server)
{
    int received;
    int error;

    received = miniftpd_recv(server->socket_base,
                             server->session.control_fd,
                             g_control_recv, sizeof(g_control_recv), 0);
    if (received > 0) {
        server->session.idle_seconds = 0;
        return consume_control_data(server, g_control_recv, received);
    }
    if (received == 0)
        return 0;
    error = miniftpd_socket_errno(server->socket_base);
    return socket_would_block(error);
}

static void accept_data_client(struct MiniFtpdServer *server)
{
    struct MiniFtpdSockAddr peer_address;
    int peer_length;
    int fd;

    peer_length = sizeof(peer_address);
    fd = miniftpd_accept(server->socket_base,
                         server->session.passive_listen_fd,
                         &peer_address, &peer_length);
    if (fd < 0)
        return;
    if (!set_nonblocking(server, fd)) {
        miniftpd_close_socket(server->socket_base, fd);
        return;
    }
    close_session_socket(server, &server->session.data_fd);
    server->session.data_fd = fd;
    close_session_socket(server, &server->session.passive_listen_fd);
    console_write("FTP passive data client connected.\n");
}

static void accept_client(struct MiniFtpdServer *server)
{
    struct MiniFtpdSockAddr peer_address;
    int peer_length;
    int fd;

    peer_length = sizeof(peer_address);
    fd = miniftpd_accept(server->socket_base, server->listen_fd,
                         &peer_address, &peer_length);
    if (fd < 0) {
        console_number("accept() failed, errno=",
                       miniftpd_socket_errno(server->socket_base));
        return;
    }
    if (!set_nonblocking(server, fd)) {
        miniftpd_close_socket(server->socket_base, fd);
        console_write("Could not set accepted socket non-blocking.\n");
        return;
    }
    if (server->session.control_fd >= 0) {
        send_reply(server, fd, "421 MiniFTPD is busy.\r\n");
        miniftpd_close_socket(server->socket_base, fd);
        return;
    }
    reset_session(&server->session);
    server->session.control_fd = fd;
    server->session.connection_state = MINIFTPD_SESSION_CONNECTED;
    server->session.idle_seconds = 0;
    server->command_overflow = 0;
    console_write("FTP control client connected.\n");
    if (!send_reply(server, fd, "220 MiniFTPD ready.\r\n"))
        close_client(server);
}

static int open_listener(struct MiniFtpdServer *server)
{
    struct MiniFtpdSockAddrIn address;

    server->listen_fd = miniftpd_socket(server->socket_base,
                                        MINIFTPD_AF_INET,
                                        MINIFTPD_SOCK_STREAM,
                                        MINIFTPD_IPPROTO_TCP);
    if (server->listen_fd < 0) {
        console_number("socket() failed, errno=",
                       miniftpd_socket_errno(server->socket_base));
        return 0;
    }
    memset(&address, 0, sizeof(address));
    address.sin_len = sizeof(address);
    address.sin_family = MINIFTPD_AF_INET;
    address.sin_port = server->config->port;
    address.sin_addr.s_addr = MINIFTPD_INADDR_ANY;
    if (miniftpd_bind(server->socket_base, server->listen_fd,
                      (const struct MiniFtpdSockAddr *)&address,
                      sizeof(address)) < 0) {
        console_number("bind() failed, errno=",
                       miniftpd_socket_errno(server->socket_base));
        return 0;
    }
    if (miniftpd_listen(server->socket_base, server->listen_fd, 1) < 0) {
        console_number("listen() failed, errno=",
                       miniftpd_socket_errno(server->socket_base));
        return 0;
    }
    return 1;
}

int miniftpd_server_run(struct Library *socket_base,
                        const struct MiniFtpdConfig *config)
{
    struct MiniFtpdServer server;
    ULONG signals;
    int highest_fd;
    int ready;
    int running;
    char line[96];

    memset(&server, 0, sizeof(server));
    server.socket_base = socket_base;
    server.config = config;
    server.listen_fd = -1;
    server.next_pasv_port = config->pasv_port_min;
    reset_session(&server.session);
    if (!miniftpd_path_root_valid(config->root)) {
        console_write("Configured FTP root is not an accessible directory.\n");
        return 0;
    }
    if (!open_listener(&server)) {
        if (server.listen_fd >= 0)
            miniftpd_close_socket(socket_base, server.listen_fd);
        return 0;
    }
    sprintf(line, "MiniFTPD listening on port %u. Press Ctrl-C to stop.\n",
            (unsigned)config->port);
    console_write(line);
    running = 1;
    while (running) {
        if (ctrl_c_pending())
            break;
        MINIFTPD_FD_ZERO(&g_readfds);
        MINIFTPD_FD_SET(server.listen_fd, &g_readfds);
        highest_fd = server.listen_fd;
        if (server.session.control_fd >= 0) {
            MINIFTPD_FD_SET(server.session.control_fd, &g_readfds);
            if (server.session.control_fd > highest_fd)
                highest_fd = server.session.control_fd;
        }
        if (server.session.passive_listen_fd >= 0) {
            MINIFTPD_FD_SET(server.session.passive_listen_fd, &g_readfds);
            if (server.session.passive_listen_fd > highest_fd)
                highest_fd = server.session.passive_listen_fd;
        }
        signals = SIGBREAKF_CTRL_C;
        g_timeout.tv_sec = 1;
        g_timeout.tv_usec = 0;
        ready = miniftpd_wait_select(socket_base, highest_fd + 1,
                                     &g_readfds, 0, 0, &g_timeout, &signals);
        if ((signals & SIGBREAKF_CTRL_C) || ctrl_c_pending()) {
            running = 0;
            continue;
        }
        if (ready < 0) {
            console_number("WaitSelect() failed, errno=",
                           miniftpd_socket_errno(socket_base));
            break;
        }
        if (ready == 0 &&
            server.session.connection_state == MINIFTPD_SESSION_CONNECTED) {
            if (server.session.idle_seconds < 65535)
                ++server.session.idle_seconds;
            if (server.session.idle_seconds >= config->timeout_seconds) {
                send_reply(&server, server.session.control_fd,
                           "421 Control connection timed out.\r\n");
                close_client(&server);
            }
            continue;
        }
        if (MINIFTPD_FD_ISSET(server.listen_fd, &g_readfds))
            accept_client(&server);
        if (server.stop_requested) {
            running = 0;
            continue;
        }
        if (server.session.passive_listen_fd >= 0 &&
            MINIFTPD_FD_ISSET(server.session.passive_listen_fd, &g_readfds))
            accept_data_client(&server);
        if (server.session.control_fd >= 0 &&
            MINIFTPD_FD_ISSET(server.session.control_fd, &g_readfds) &&
            !receive_control(&server))
            close_client(&server);
        if (server.stop_requested)
            running = 0;
    }
    if (server.session.control_fd >= 0)
        close_client(&server);
    miniftpd_close_socket(socket_base, server.listen_fd);
    console_write("MiniFTPD stopped.\n");
    return 1;
}
