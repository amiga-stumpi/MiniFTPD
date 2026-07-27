#ifndef MINIFTPD_SERVER_H
#define MINIFTPD_SERVER_H

#include <exec/libraries.h>
#include "miniftpd/config.h"

int miniftpd_server_run(struct Library *socket_base,
                        const struct MiniFtpdConfig *config);

#endif
