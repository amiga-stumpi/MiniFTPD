#ifndef MINIFTPD_CONFIG_H
#define MINIFTPD_CONFIG_H

#include <exec/types.h>

#define MINIFTPD_PATH_SIZE 256
#define MINIFTPD_USER_SIZE 32
#define MINIFTPD_PASSWORD_SIZE 64
#define MINIFTPD_CONFIG_PATH_SIZE 256

struct MiniFtpdConfig
{
    UWORD port;
    char root[MINIFTPD_PATH_SIZE];
    char user[MINIFTPD_USER_SIZE];
    char password[MINIFTPD_PASSWORD_SIZE];
    UBYTE anonymous;
    UBYTE readonly;
    UWORD pasv_port_min;
    UWORD pasv_port_max;
    UWORD timeout_seconds;
    UBYTE log_enabled;
};

void miniftpd_config_defaults(struct MiniFtpdConfig *cfg);
int miniftpd_config_make_path(char *path, int path_size, const char *program_path);
int miniftpd_config_load_or_create(const char *path, struct MiniFtpdConfig *cfg,
                                   char *error, int error_size, int *created);
void miniftpd_config_print(const char *path, const struct MiniFtpdConfig *cfg);

#endif
