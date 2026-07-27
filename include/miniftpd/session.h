#ifndef MINIFTPD_SESSION_H
#define MINIFTPD_SESSION_H

#include <exec/types.h>

#define MINIFTPD_COMMAND_SIZE 512
#define MINIFTPD_CWD_SIZE 256

enum MiniFtpdConnectionState
{
    MINIFTPD_SESSION_DISCONNECTED = 0,
    MINIFTPD_SESSION_CONNECTED
};

enum MiniFtpdTransferMode
{
    MINIFTPD_TRANSFER_ASCII = 0,
    MINIFTPD_TRANSFER_BINARY
};

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
    enum MiniFtpdConnectionState connection_state;
    enum MiniFtpdLoginState login_state;
    enum MiniFtpdTransferMode transfer_mode;
    UWORD idle_seconds;
    char cwd[MINIFTPD_CWD_SIZE];
    char command[MINIFTPD_COMMAND_SIZE];
    UWORD command_len;
};

#endif
