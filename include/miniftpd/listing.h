#ifndef MINIFTPD_LISTING_H
#define MINIFTPD_LISTING_H

typedef int (*MiniFtpdListingWriter)(void *context,
                                     const char *data, int length);

int miniftpd_list_directory(const char *root, const char *virtual_path,
                            MiniFtpdListingWriter writer, void *context);

#endif
