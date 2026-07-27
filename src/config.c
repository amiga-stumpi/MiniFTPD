#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

#include "miniftpd/app.h"
#include "miniftpd/config.h"

#define CONFIG_BUFFER_SIZE 2048

static char g_config_buffer[CONFIG_BUFFER_SIZE];

static void copy_text(char *dst, int size, const char *src)
{
    int n;

    if (!dst || size <= 0)
        return;
    if (!src)
        src = "";
    n = 0;
    while (src[n] && n < size - 1) {
        dst[n] = src[n];
        ++n;
    }
    dst[n] = '\0';
}

static void set_error(char *error, int size, const char *text)
{
    copy_text(error, size, text);
}

static char *trim_text(char *text)
{
    char *start;
    char *end;

    if (!text)
        return text;
    start = text;
    while (*start == ' ' || *start == '\t')
        ++start;
    end = start + strlen(start);
    while (end > start &&
           (end[-1] == ' ' || end[-1] == '\t' ||
            end[-1] == '\r' || end[-1] == '\n'))
        --end;
    *end = '\0';
    return start;
}

static int parse_unsigned(const char *text, ULONG min_value,
                          ULONG max_value, ULONG *value)
{
    ULONG result;
    int digits;

    if (!text || !value)
        return 0;
    result = 0;
    digits = 0;
    while (*text >= '0' && *text <= '9') {
        if (result > (max_value - (ULONG)(*text - '0')) / 10UL)
            return 0;
        result = result * 10UL + (ULONG)(*text - '0');
        ++digits;
        ++text;
    }
    while (*text == ' ' || *text == '\t')
        ++text;
    if (!digits || *text || result < min_value || result > max_value)
        return 0;
    *value = result;
    return 1;
}

static int parse_boolean(const char *text, UBYTE *value)
{
    ULONG number;

    if (!parse_unsigned(text, 0, 1, &number))
        return 0;
    *value = (UBYTE)number;
    return 1;
}

static int path_has_separator(const char *path)
{
    while (path && *path) {
        if (*path == '/' || *path == ':')
            return 1;
        ++path;
    }
    return 0;
}

void miniftpd_config_defaults(struct MiniFtpdConfig *cfg)
{
    if (!cfg)
        return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->port = 21;
    copy_text(cfg->root, sizeof(cfg->root), "RAM:");
    copy_text(cfg->user, sizeof(cfg->user), "amiga");
    copy_text(cfg->password, sizeof(cfg->password), "amiga");
    cfg->anonymous = 0;
    cfg->readonly = 0;
    cfg->pasv_port_min = 50000;
    cfg->pasv_port_max = 50020;
    cfg->timeout_seconds = 120;
    cfg->log_enabled = 0;
}

int miniftpd_config_make_path(char *path, int path_size,
                              const char *program_path)
{
    int length;
    int split;
    int name_length;

    if (!path || path_size <= 0)
        return 0;
    if (!program_path || !path_has_separator(program_path)) {
        copy_text(path, path_size, MINIFTPD_CONFIG_FILE);
        return strlen(path) == strlen(MINIFTPD_CONFIG_FILE);
    }
    length = strlen(program_path);
    split = length;
    while (split > 0 && program_path[split - 1] != '/' &&
           program_path[split - 1] != ':')
        --split;
    name_length = strlen(MINIFTPD_CONFIG_FILE);
    if (split + name_length >= path_size)
        return 0;
    memcpy(path, program_path, split);
    memcpy(path + split, MINIFTPD_CONFIG_FILE, name_length + 1);
    return 1;
}

static int write_line(BPTR file, const char *line)
{
    LONG length;

    length = (LONG)strlen(line);
    return Write(file, (APTR)line, length) == length;
}

static int write_default_file(const char *path,
                              const struct MiniFtpdConfig *cfg)
{
    char line[128];
    BPTR file;

    file = Open((STRPTR)path, MODE_NEWFILE);
    if (!file)
        return 0;
    if (!write_line(file, "# MiniFTPD configuration\n") ||
        !write_line(file, "# Passwords are never printed or logged.\n"))
        goto write_failed;
    sprintf(line, "port=%u\n", (unsigned)cfg->port);
    if (!write_line(file, line)) goto write_failed;
    sprintf(line, "root=%s\n", cfg->root);
    if (!write_line(file, line)) goto write_failed;
    sprintf(line, "user=%s\n", cfg->user);
    if (!write_line(file, line)) goto write_failed;
    sprintf(line, "password=%s\n", cfg->password);
    if (!write_line(file, line)) goto write_failed;
    sprintf(line, "anonymous=%u\n", (unsigned)cfg->anonymous);
    if (!write_line(file, line)) goto write_failed;
    sprintf(line, "readonly=%u\n", (unsigned)cfg->readonly);
    if (!write_line(file, line)) goto write_failed;
    sprintf(line, "pasv_port_min=%u\n", (unsigned)cfg->pasv_port_min);
    if (!write_line(file, line)) goto write_failed;
    sprintf(line, "pasv_port_max=%u\n", (unsigned)cfg->pasv_port_max);
    if (!write_line(file, line)) goto write_failed;
    sprintf(line, "timeout=%u\n", (unsigned)cfg->timeout_seconds);
    if (!write_line(file, line)) goto write_failed;
    sprintf(line, "log_enabled=%u\n", (unsigned)cfg->log_enabled);
    if (!write_line(file, line)) goto write_failed;
    Close(file);
    return 1;

write_failed:
    Close(file);
    return 0;
}

static int parse_line(char *line, struct MiniFtpdConfig *cfg,
                      char *error, int error_size)
{
    char *key;
    char *value;
    char *equals;
    ULONG number;

    key = trim_text(line);
    if (!key[0] || key[0] == '#')
        return 1;
    equals = strchr(key, '=');
    if (!equals) {
        set_error(error, error_size, "expected key=value");
        return 0;
    }
    *equals = '\0';
    value = trim_text(equals + 1);
    key = trim_text(key);

    if (!strcmp(key, "port")) {
        if (!parse_unsigned(value, 1, 65535, &number)) goto invalid_value;
        cfg->port = (UWORD)number;
    } else if (!strcmp(key, "root")) {
        if (!value[0] || strlen(value) >= sizeof(cfg->root)) goto invalid_value;
        copy_text(cfg->root, sizeof(cfg->root), value);
    } else if (!strcmp(key, "user")) {
        if (!value[0] || strlen(value) >= sizeof(cfg->user)) goto invalid_value;
        copy_text(cfg->user, sizeof(cfg->user), value);
    } else if (!strcmp(key, "password")) {
        if (strlen(value) >= sizeof(cfg->password)) goto invalid_value;
        copy_text(cfg->password, sizeof(cfg->password), value);
    } else if (!strcmp(key, "anonymous")) {
        if (!parse_boolean(value, &cfg->anonymous)) goto invalid_value;
    } else if (!strcmp(key, "readonly")) {
        if (!parse_boolean(value, &cfg->readonly)) goto invalid_value;
    } else if (!strcmp(key, "pasv_port_min")) {
        if (!parse_unsigned(value, 1024, 65535, &number)) goto invalid_value;
        cfg->pasv_port_min = (UWORD)number;
    } else if (!strcmp(key, "pasv_port_max")) {
        if (!parse_unsigned(value, 1024, 65535, &number)) goto invalid_value;
        cfg->pasv_port_max = (UWORD)number;
    } else if (!strcmp(key, "timeout")) {
        if (!parse_unsigned(value, 10, 3600, &number)) goto invalid_value;
        cfg->timeout_seconds = (UWORD)number;
    } else if (!strcmp(key, "log_enabled")) {
        if (!parse_boolean(value, &cfg->log_enabled)) goto invalid_value;
    } else {
        set_error(error, error_size, "unknown configuration key");
        return 0;
    }
    return 1;

invalid_value:
    set_error(error, error_size, "invalid configuration value");
    return 0;
}

static int parse_buffer(struct MiniFtpdConfig *cfg, LONG length,
                        char *error, int error_size)
{
    LONG position;
    LONG start;

    position = 0;
    while (position < length) {
        start = position;
        while (position < length && g_config_buffer[position] != '\n' &&
               g_config_buffer[position] != '\r')
            ++position;
        g_config_buffer[position] = '\0';
        if (!parse_line(g_config_buffer + start, cfg, error, error_size))
            return 0;
        ++position;
        while (position < length &&
               (g_config_buffer[position] == '\n' ||
                g_config_buffer[position] == '\r'))
            ++position;
    }
    return 1;
}

int miniftpd_config_load_or_create(const char *path,
                                   struct MiniFtpdConfig *cfg,
                                   char *error,
                                   int error_size,
                                   int *created)
{
    BPTR file;
    LONG length;
    LONG extra;

    if (created)
        *created = 0;
    set_error(error, error_size, "");
    if (!path || !cfg) {
        set_error(error, error_size, "invalid configuration context");
        return 0;
    }
    miniftpd_config_defaults(cfg);
    file = Open((STRPTR)path, MODE_OLDFILE);
    if (!file) {
        if (!write_default_file(path, cfg)) {
            set_error(error, error_size, "cannot create miniftpd.conf");
            return 0;
        }
        if (created)
            *created = 1;
        return 1;
    }
    length = Read(file, g_config_buffer, sizeof(g_config_buffer) - 1);
    if (length < 0) {
        Close(file);
        set_error(error, error_size, "cannot read miniftpd.conf");
        return 0;
    }
    extra = Read(file, g_config_buffer + sizeof(g_config_buffer) - 1, 1);
    Close(file);
    if (extra > 0) {
        set_error(error, error_size, "miniftpd.conf is too large");
        return 0;
    }
    g_config_buffer[length] = '\0';
    if (!parse_buffer(cfg, length, error, error_size))
        return 0;
    if (cfg->pasv_port_min > cfg->pasv_port_max) {
        set_error(error, error_size, "passive port range is reversed");
        return 0;
    }
    if (!cfg->anonymous && (!cfg->user[0] || !cfg->password[0])) {
        set_error(error, error_size, "user and password are required");
        return 0;
    }
    return 1;
}

static void console_write(const char *text)
{
    BPTR output;

    output = Output();
    if (output && text)
        Write(output, (APTR)text, (LONG)strlen(text));
}

void miniftpd_config_print(const char *path, const struct MiniFtpdConfig *cfg)
{
    char line[320];

    if (!cfg)
        return;
    sprintf(line, "Config: %s\n", path);
    console_write(line);
    sprintf(line, "Port: %u  Root: %s\n", (unsigned)cfg->port, cfg->root);
    console_write(line);
    sprintf(line, "User: %s  Anonymous: %s  Read-only: %s\n",
            cfg->user, cfg->anonymous ? "yes" : "no",
            cfg->readonly ? "yes" : "no");
    console_write(line);
    sprintf(line, "Passive ports: %u-%u  Timeout: %u seconds\n",
            (unsigned)cfg->pasv_port_min, (unsigned)cfg->pasv_port_max,
            (unsigned)cfg->timeout_seconds);
    console_write(line);
}
