#ifndef MINIFTPD_CONFIG_H
#define MINIFTPD_CONFIG_H

#include <exec/types.h>

#define MINIFTPD_PATH_SIZE 256
#define MINIFTPD_USER_SIZE 32
#define MINIFTPD_PASSWORD_SIZE 64

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

#endif
