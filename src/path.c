#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <string.h>

#include "miniftpd/path.h"

static char g_dos_path[MINIFTPD_PATH_SIZE * 2];
static struct FileInfoBlock g_path_fib;

static int append_component(char *output, int output_size,
                            const char *start, int length)
{
    int current;

    if (length <= 0)
        return 1;
    current = strlen(output);
    if (current > 1) {
        if (current + 1 >= output_size)
            return 0;
        output[current++] = '/';
    }
    if (current + length >= output_size)
        return 0;
    memcpy(output + current, start, length);
    output[current + length] = '\0';
    return 1;
}

static int remove_component(char *output)
{
    int length;

    length = strlen(output);
    if (length <= 1)
        return 0;
    while (length > 1 && output[length - 1] != '/')
        --length;
    if (length > 1)
        --length;
    output[length] = '\0';
    return 1;
}

static int apply_path(const char *input, char *output, int output_size)
{
    const char *component;
    const char *cursor;
    int length;

    cursor = input;
    while (*cursor) {
        while (*cursor == '/')
            ++cursor;
        if (!*cursor)
            break;
        component = cursor;
        while (*cursor && *cursor != '/') {
            unsigned char value = (unsigned char)*cursor;

            if (value < 32 || value == 127 || value == ':' || value == '"')
                return 0;
            ++cursor;
        }
        length = (int)(cursor - component);
        if (length == 1 && component[0] == '.')
            continue;
        if (length == 2 && component[0] == '.' && component[1] == '.') {
            if (!remove_component(output))
                return 0;
            continue;
        }
        if (!append_component(output, output_size, component, length))
            return 0;
    }
    return 1;
}

int miniftpd_path_normalize(const char *cwd, const char *input,
                            char *output, int output_size)
{
    if (!cwd || !input || !output || output_size < 2 || !input[0])
        return 0;
    output[0] = '/';
    output[1] = '\0';
    if (input[0] != '/' && !apply_path(cwd, output, output_size))
        return 0;
    return apply_path(input, output, output_size);
}

int miniftpd_path_build_dos(const char *root, const char *virtual_path,
                            char *output, int output_size)
{
    int root_length;
    int path_length;
    int add_separator;
    const char *relative;

    if (!root || !root[0] || !virtual_path || virtual_path[0] != '/' ||
        !output || output_size <= 0)
        return 0;
    root_length = strlen(root);
    relative = virtual_path + 1;
    path_length = strlen(relative);
    add_separator = path_length > 0 && root[root_length - 1] != ':' &&
                    root[root_length - 1] != '/';
    if (root_length + add_separator + path_length >= output_size)
        return 0;
    memcpy(output, root, root_length);
    if (add_separator)
        output[root_length++] = '/';
    memcpy(output + root_length, relative, path_length + 1);
    return 1;
}

static int locks_equal(BPTR left, BPTR right)
{
    const struct FileLock *a;
    const struct FileLock *b;

    if (!left || !right)
        return 0;
    a = (const struct FileLock *)BADDR(left);
    b = (const struct FileLock *)BADDR(right);
    return a->fl_Key == b->fl_Key && a->fl_Task == b->fl_Task &&
           a->fl_Volume == b->fl_Volume;
}

static int lock_is_beneath(BPTR root_lock, BPTR target_lock)
{
    BPTR cursor;
    BPTR parent;
    int inside;

    cursor = DupLock(target_lock);
    inside = 0;
    while (cursor) {
        if (locks_equal(root_lock, cursor)) {
            inside = 1;
            break;
        }
        parent = ParentDir(cursor);
        UnLock(cursor);
        cursor = parent;
    }
    if (cursor)
        UnLock(cursor);
    return inside;
}

int miniftpd_path_root_valid(const char *root)
{
    BPTR lock;
    int valid;

    if (!root || !root[0])
        return 0;
    lock = Lock((STRPTR)root, SHARED_LOCK);
    if (!lock)
        return 0;
    valid = Examine(lock, &g_path_fib) && g_path_fib.fib_DirEntryType > 0;
    UnLock(lock);
    return valid;
}

int miniftpd_path_resolve_file(const char *root, const char *virtual_path,
                               char *dos_path, int dos_path_size,
                               LONG *file_size)
{
    BPTR root_lock;
    BPTR target_lock;
    int valid;

    if (file_size)
        *file_size = 0;
    if (!miniftpd_path_build_dos(root, virtual_path,
                                 dos_path, dos_path_size))
        return 0;
    root_lock = Lock((STRPTR)root, SHARED_LOCK);
    if (!root_lock)
        return 0;
    target_lock = Lock((STRPTR)dos_path, SHARED_LOCK);
    if (!target_lock) {
        UnLock(root_lock);
        return 0;
    }
    valid = lock_is_beneath(root_lock, target_lock) &&
            Examine(target_lock, &g_path_fib) &&
            g_path_fib.fib_DirEntryType <= 0;
    if (valid && file_size)
        *file_size = g_path_fib.fib_Size;
    UnLock(target_lock);
    UnLock(root_lock);
    return valid;
}

int miniftpd_path_resolve_upload(const char *root, const char *virtual_path,
                                 char *dos_path, int dos_path_size)
{
    char parent[MINIFTPD_PATH_SIZE];
    BPTR target_lock;
    int length;

    if (!root || !virtual_path || virtual_path[0] != '/' ||
        !virtual_path[1] || !dos_path)
        return 0;
    length = strlen(virtual_path);
    if (length >= (int)sizeof(parent) || virtual_path[length - 1] == '/')
        return 0;
    memcpy(parent, virtual_path, length + 1);
    while (length > 1 && parent[length - 1] != '/')
        --length;
    if (length > 1)
        --length;
    parent[length] = '\0';
    if (!miniftpd_path_directory_exists(root, parent) ||
        !miniftpd_path_build_dos(root, virtual_path,
                                 dos_path, dos_path_size))
        return 0;
    target_lock = Lock((STRPTR)dos_path, SHARED_LOCK);
    if (target_lock) {
        int is_directory;

        is_directory = Examine(target_lock, &g_path_fib) &&
                       g_path_fib.fib_DirEntryType > 0;
        UnLock(target_lock);
        if (is_directory)
            return 0;
    }
    return 1;
}

int miniftpd_path_directory_exists(const char *root,
                                   const char *virtual_path)
{
    BPTR root_lock;
    BPTR target_lock;
    int valid;

    if (!miniftpd_path_build_dos(root, virtual_path,
                                 g_dos_path, sizeof(g_dos_path)))
        return 0;
    root_lock = Lock((STRPTR)root, SHARED_LOCK);
    if (!root_lock)
        return 0;
    target_lock = Lock((STRPTR)g_dos_path, SHARED_LOCK);
    if (!target_lock) {
        UnLock(root_lock);
        return 0;
    }
    valid = lock_is_beneath(root_lock, target_lock) &&
            Examine(target_lock, &g_path_fib) &&
            g_path_fib.fib_DirEntryType > 0;
    UnLock(target_lock);
    UnLock(root_lock);
    return valid;
}

int miniftpd_path_writes_allowed(const struct MiniFtpdConfig *config)
{
    return config && !config->readonly;
}
