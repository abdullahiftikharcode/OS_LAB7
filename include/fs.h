#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <stdbool.h>
#include "user.h"

bool fs_upload(UserStore *store, const char *user, const char *dst_relpath, const char *tmp_src, size_t size, char *err, size_t errlen);
bool fs_download_path(UserStore *store, const char *user, const char *relpath, char *out_path, size_t out_len, char *err, size_t errlen);
bool fs_delete(UserStore *store, const char *user, const char *relpath, char *err, size_t errlen);
// Writes listing into buffer separated by \n
bool fs_list(UserStore *store, const char *user, char *buf, size_t buflen, char *err, size_t errlen);

#endif

