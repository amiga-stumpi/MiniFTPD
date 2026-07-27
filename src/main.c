#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

#include "miniftpd/app.h"
#include "miniftpd/config.h"
#include "miniftpd/server.h"

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

int main(int argc, char **argv)
{
    struct Library *socket_base;
    static struct MiniFtpdConfig config;
    static char config_path[MINIFTPD_CONFIG_PATH_SIZE];
    static char config_error[128];
    int config_created;
    const char *program_path;

    (void)version_tag;
    console_puts(MINIFTPD_NAME " " MINIFTPD_VERSION "\n");
    console_puts("AmigaOS 1.3 FTP server project\n");

    program_path = (argc > 0 && argv && argv[0]) ? argv[0] : "";
    if (!miniftpd_config_make_path(config_path, sizeof(config_path),
                                   program_path)) {
        console_puts("Error: program path is too long.\n");
        return 20;
    }
    if (!miniftpd_config_load_or_create(config_path, &config,
                                        config_error, sizeof(config_error),
                                        &config_created)) {
        console_puts("Configuration error: ");
        console_puts(config_error);
        console_puts("\n");
        return 20;
    }
    if (config_created)
        console_puts("Created default miniftpd.conf.\n");
    miniftpd_config_print(config_path, &config);

    socket_base = OpenLibrary((STRPTR)"bsdsocket.library", 0);
    if (!socket_base) {
        console_puts("Error: bsdsocket.library is not available.\n");
        return 20;
    }
    console_puts("bsdsocket.library found.\n");
    if (!miniftpd_server_run(socket_base, &config)) {
        console_puts("MiniFTPD server failed.\n");
        CloseLibrary(socket_base);
        return 20;
    }
    CloseLibrary(socket_base);
    return 0;
}
