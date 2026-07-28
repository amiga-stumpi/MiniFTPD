#ifndef MINIFTPD_TRANSFER_H
#define MINIFTPD_TRANSFER_H

#include <exec/types.h>

typedef int (*MiniFtpdTransferWriter)(void *context,
                                      const UBYTE *data, int length);

#define MINIFTPD_RETR_OK           1
#define MINIFTPD_RETR_NOT_FOUND    0
#define MINIFTPD_RETR_READ_ERROR  -1
#define MINIFTPD_RETR_WRITE_ERROR -2
#define MINIFTPD_RETR_NO_MEMORY   -3

int miniftpd_retrieve_file(const char *root, const char *virtual_path,
                           MiniFtpdTransferWriter writer, void *context,
                           LONG *file_size);

#endif
