#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

#include "miniftpd/config.h"
#include "miniftpd/server.h"
#include "miniftpd/session.h"
#include "miniftpd/socket_api.h"

#define CONTROL_RECV_SIZE 256
#define SEND_TIMEOUT_SECONDS 5

struct MiniFtpdServer
{
    struct Library *socket_base;
    const struct MiniFtpdConfig *config;
    int listen_fd;
    struct MiniFtpdSession session;
    UBYTE command_overflow;
    UBYTE stop_requested;
};

static UBYTE g_control_recv[CONTROL_RECV_SIZE];
static struct MiniFtpdFdSet g_readfds;
static struct MiniFtpdFdSet g_writefds;
static struct MiniFtpdTimeVal g_timeout;

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
    if (signals & SIGBREAKF_CTRL_C) {
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

static void accept_client(struct MiniFtpdServer *server)
{
    struct MiniFtpdSockAddr peer_address;
    int peer_length;
    int fd;
    ULONG nonblocking;

    peer_length = sizeof(peer_address);
    fd = miniftpd_accept(server->socket_base, server->listen_fd,
                         &peer_address, &peer_length);
    if (fd < 0) {
        console_number("accept() failed, errno=",
                       miniftpd_socket_errno(server->socket_base));
        return;
    }
    nonblocking = 1;
    if (miniftpd_ioctl(server->socket_base, fd, MINIFTPD_FIONBIO,
                       &nonblocking) < 0) {
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
    reset_session(&server.session);
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
        MINIFTPD_FD_ZERO(&g_readfds);
        MINIFTPD_FD_SET(server.listen_fd, &g_readfds);
        highest_fd = server.listen_fd;
        if (server.session.control_fd >= 0) {
            MINIFTPD_FD_SET(server.session.control_fd, &g_readfds);
            if (server.session.control_fd > highest_fd)
                highest_fd = server.session.control_fd;
        }
        signals = SIGBREAKF_CTRL_C;
        if (server.session.connection_state == MINIFTPD_SESSION_CONNECTED) {
            g_timeout.tv_sec = 1;
            g_timeout.tv_usec = 0;
            ready = miniftpd_wait_select(socket_base, highest_fd + 1,
                                         &g_readfds, 0, 0, &g_timeout, &signals);
        } else {
            ready = miniftpd_wait_select(socket_base, highest_fd + 1,
                                         &g_readfds, 0, 0, 0, &signals);
        }
        if (signals & SIGBREAKF_CTRL_C) {
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
