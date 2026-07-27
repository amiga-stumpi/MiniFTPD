#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

#include "miniftpd/app.h"

static const char version_tag[] =
    "$VER: " MINIFTPD_NAME " " MINIFTPD_VERSION " (27.07.2026)";

static void console_puts(const char *text)
{
    BPTR output;

    if (!text)
        return;

    output = Output();
    if (output)
        Write(output, (APTR)text, (LONG)strlen(text));
}

int main(void)
{
    struct Library *socket_base;

    (void)version_tag;

    console_puts(MINIFTPD_NAME " " MINIFTPD_VERSION "\n");
    console_puts("AmigaOS 1.3 FTP server project\n");

    socket_base = OpenLibrary((STRPTR)"bsdsocket.library", 0);
    if (!socket_base) {
        console_puts("Error: bsdsocket.library is not available.\n");
        return 20;
    }

    console_puts("bsdsocket.library found.\n");
    console_puts("Project step 1 ready. FTP listener is not implemented yet.\n");

    CloseLibrary(socket_base);
    return 0;
}
