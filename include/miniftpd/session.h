#ifndef MINIFTPD_SESSION_H
#define MINIFTPD_SESSION_H

#include <exec/types.h>

#define MINIFTPD_COMMAND_SIZE 512
#define MINIFTPD_CWD_SIZE 256

enum MiniFtpdLoginState
{
    MINIFTPD_LOGIN_NONE = 0,
    MINIFTPD_LOGIN_USER,
    MINIFTPD_LOGIN_AUTHENTICATED
};

struct MiniFtpdSession
{
    int control_fd;
    int passive_listen_fd;
    int data_fd;
    enum MiniFtpdLoginState login_state;
    UBYTE binary_mode;
    char cwd[MINIFTPD_CWD_SIZE];
    char command[MINIFTPD_COMMAND_SIZE];
    UWORD command_len;
};

#endif
