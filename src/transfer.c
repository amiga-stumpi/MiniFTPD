#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include "miniftpd/config.h"
#include "miniftpd/path.h"
#include "miniftpd/transfer.h"

#define RETR_BUFFER_SIZE 16384
#define STOR_BUFFER_SIZE 16384

static char g_retr_path[MINIFTPD_PATH_SIZE * 2];
static char g_stor_path[MINIFTPD_PATH_SIZE * 2];

int miniftpd_retrieve_file(const char *root, const char *virtual_path,
                           MiniFtpdTransferWriter writer, void *context,
                           LONG restart_offset, LONG *file_size)
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
    if (restart_offset < 0 || restart_offset > size ||
        (restart_offset && Seek(file, restart_offset, OFFSET_BEGINNING) < 0)) {
        Close(file);
        return MINIFTPD_RETR_NOT_FOUND;
    }
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
        *file_size = size - restart_offset;
    return result;
}

int miniftpd_store_file(const char *root, const char *virtual_path,
                        MiniFtpdTransferReader reader, void *context,
                        LONG restart_offset, LONG *file_size)
{
    UBYTE *buffer;
    BPTR file;
    LONG received;
    LONG written;
    LONG total;
    int result;

    if (file_size)
        *file_size = 0;
    if (!root || !virtual_path || !reader)
        return MINIFTPD_STOR_BAD_PATH;
    if (!miniftpd_path_resolve_upload(root, virtual_path,
                                      g_stor_path, sizeof(g_stor_path)))
        return MINIFTPD_STOR_BAD_PATH;
    if (restart_offset < 0)
        return MINIFTPD_STOR_BAD_PATH;
    buffer = (UBYTE *)AllocMem(STOR_BUFFER_SIZE, MEMF_PUBLIC);
    if (!buffer)
        return MINIFTPD_STOR_NO_MEMORY;
    if (restart_offset) {
        LONG existing_size;

        if (!miniftpd_path_resolve_file(root, virtual_path,
                                        g_stor_path, sizeof(g_stor_path),
                                        &existing_size) ||
            restart_offset > existing_size) {
            FreeMem(buffer, STOR_BUFFER_SIZE);
            return MINIFTPD_STOR_BAD_PATH;
        }
        file = Open((STRPTR)g_stor_path, MODE_OLDFILE);
    } else {
        file = Open((STRPTR)g_stor_path, MODE_NEWFILE);
    }
    if (!file) {
        FreeMem(buffer, STOR_BUFFER_SIZE);
        return MINIFTPD_STOR_OPEN_ERROR;
    }
    if (restart_offset && Seek(file, restart_offset, OFFSET_BEGINNING) < 0) {
        FreeMem(buffer, STOR_BUFFER_SIZE);
        Close(file);
        return MINIFTPD_STOR_OPEN_ERROR;
    }
    total = 0;
    result = MINIFTPD_STOR_OK;
    for (;;) {
        received = reader(context, buffer, STOR_BUFFER_SIZE);
        if (received < 0) {
            result = MINIFTPD_STOR_READ_ERROR;
            break;
        }
        if (received == 0)
            break;
        written = Write(file, buffer, received);
        if (written != received) {
            result = MINIFTPD_STOR_WRITE_ERROR;
            break;
        }
        total += received;
    }
    FreeMem(buffer, STOR_BUFFER_SIZE);
    Close(file);
    if (file_size)
        *file_size = total;
    return result;
}
