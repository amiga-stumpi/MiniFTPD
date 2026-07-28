#ifndef MINIFTPD_PATH_H
#define MINIFTPD_PATH_H

#include "miniftpd/config.h"

int miniftpd_path_normalize(const char *cwd, const char *input,
                            char *output, int output_size);
int miniftpd_path_build_dos(const char *root, const char *virtual_path,
                            char *output, int output_size);
int miniftpd_path_root_valid(const char *root);
int miniftpd_path_resolve_file(const char *root, const char *virtual_path,
                               char *dos_path, int dos_path_size,
                               LONG *file_size);
int miniftpd_path_resolve_upload(const char *root, const char *virtual_path,
                                 char *dos_path, int dos_path_size);
int miniftpd_path_directory_exists(const char *root,
                                   const char *virtual_path);
int miniftpd_path_delete_file(const char *root, const char *virtual_path);
int miniftpd_path_make_directory(const char *root, const char *virtual_path);
int miniftpd_path_remove_directory(const char *root, const char *virtual_path);
int miniftpd_path_rename(const char *root, const char *source_virtual,
                         const char *target_virtual);
int miniftpd_path_writes_allowed(const struct MiniFtpdConfig *config);

#endif
