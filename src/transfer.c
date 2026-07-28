#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include "miniftpd/config.h"
#include "miniftpd/path.h"
#include "miniftpd/transfer.h"

#define RETR_BUFFER_SIZE 16384

static char g_retr_path[MINIFTPD_PATH_SIZE * 2];

int miniftpd_retrieve_file(const char *root, const char *virtual_path,
                           MiniFtpdTransferWriter writer, void *context,
                           LONG *file_size)
{
    UBYTE *buffer;
    BPTR file;
    LONG size;
    LONG received;
    int result;

    if (file_size)
        *file_size = 0;
    if (!root || !virtual_path || !writer)
        return MINIFTPD_RETR_NOT_FOUND;
    if (!miniftpd_path_resolve_file(root, virtual_path,
                                    g_retr_path, sizeof(g_retr_path), &size))
        return MINIFTPD_RETR_NOT_FOUND;
    file = Open((STRPTR)g_retr_path, MODE_OLDFILE);
    if (!file)
        return MINIFTPD_RETR_NOT_FOUND;
    buffer = (UBYTE *)AllocMem(RETR_BUFFER_SIZE, MEMF_PUBLIC);
    if (!buffer) {
        Close(file);
        return MINIFTPD_RETR_NO_MEMORY;
    }
    result = MINIFTPD_RETR_OK;
    for (;;) {
        received = Read(file, buffer, RETR_BUFFER_SIZE);
        if (received < 0) {
            result = MINIFTPD_RETR_READ_ERROR;
            break;
        }
        if (received == 0)
            break;
        if (!writer(context, buffer, (int)received)) {
            result = MINIFTPD_RETR_WRITE_ERROR;
            break;
        }
    }
    FreeMem(buffer, RETR_BUFFER_SIZE);
    Close(file);
    if (result == MINIFTPD_RETR_OK && file_size)
        *file_size = size;
    return result;
}
