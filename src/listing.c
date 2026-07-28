#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

#include "miniftpd/config.h"
#include "miniftpd/listing.h"
#include "miniftpd/path.h"

#define LISTING_LINE_SIZE 320

static char g_listing_path[MINIFTPD_PATH_SIZE * 2];
static char g_listing_line[LISTING_LINE_SIZE];
static char g_listing_name[108];
static struct FileInfoBlock g_listing_fib;

static int write_entry(MiniFtpdListingWriter writer, void *context,
                       const struct FileInfoBlock *fib)
{
    const char *mode;
    int position;
    LONG size;
    int length;

    position = 0;
    while (fib->fib_FileName[position] &&
           position < (int)sizeof(g_listing_name) - 1) {
        unsigned char value = (unsigned char)fib->fib_FileName[position];

        g_listing_name[position] =
            (value < 32 || value == 127) ? '?' : (char)value;
        ++position;
    }
    g_listing_name[position] = '\0';
    mode = fib->fib_DirEntryType > 0 ? "drwxr-xr-x" : "-rw-r--r--";
    size = fib->fib_DirEntryType > 0 ? 0 : fib->fib_Size;
    length = sprintf(g_listing_line,
                     "%s 1 amiga amiga %10ld Jan 01 00:00 %s\r\n",
                     mode, (long)size, g_listing_name);
    if (length <= 0 || length >= (int)sizeof(g_listing_line))
        return 0;
    return writer(context, g_listing_line, length);
}

int miniftpd_list_directory(const char *root, const char *virtual_path,
                            MiniFtpdListingWriter writer, void *context)
{
    BPTR lock;
    LONG error;

    if (!root || !virtual_path || !writer)
        return 0;
    if (!miniftpd_path_directory_exists(root, virtual_path))
        return 0;
    if (!miniftpd_path_build_dos(root, virtual_path,
                                 g_listing_path, sizeof(g_listing_path)))
        return 0;
    lock = Lock((STRPTR)g_listing_path, SHARED_LOCK);
    if (!lock)
        return 0;
    if (!Examine(lock, &g_listing_fib) ||
        g_listing_fib.fib_DirEntryType <= 0) {
        UnLock(lock);
        return 0;
    }
    for (;;) {
        if (ExNext(lock, &g_listing_fib)) {
            if (!write_entry(writer, context, &g_listing_fib)) {
                UnLock(lock);
                return -1;
            }
            continue;
        }
        error = IoErr();
        UnLock(lock);
        return error == ERROR_NO_MORE_ENTRIES ? 1 : 0;
    }
}
