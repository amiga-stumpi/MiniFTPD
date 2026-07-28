#ifndef MINIFTPD_TRANSFER_H
#define MINIFTPD_TRANSFER_H

#include <exec/types.h>

typedef int (*MiniFtpdTransferWriter)(void *context,
                                      const UBYTE *data, int length);
typedef int (*MiniFtpdTransferReader)(void *context,
                                     UBYTE *data, int length);

#define MINIFTPD_RETR_OK           1
#define MINIFTPD_RETR_NOT_FOUND    0
#define MINIFTPD_RETR_READ_ERROR  -1
#define MINIFTPD_RETR_WRITE_ERROR -2
#define MINIFTPD_RETR_NO_MEMORY   -3

#define MINIFTPD_STOR_OK           1
#define MINIFTPD_STOR_BAD_PATH     0
#define MINIFTPD_STOR_READ_ERROR  -1
#define MINIFTPD_STOR_WRITE_ERROR -2
#define MINIFTPD_STOR_NO_MEMORY   -3
#define MINIFTPD_STOR_OPEN_ERROR  -4

int miniftpd_retrieve_file(const char *root, const char *virtual_path,
                           MiniFtpdTransferWriter writer, void *context,
                           LONG *file_size);
int miniftpd_store_file(const char *root, const char *virtual_path,
                        MiniFtpdTransferReader reader, void *context,
                        LONG *file_size);

#endif
