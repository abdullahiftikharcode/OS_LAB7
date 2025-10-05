#include "fs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

static void build_user_path(UserStore *store, const char *user, const char *rel, char *out, size_t out_len) {
	snprintf(out, out_len, "%s/%s/%s", user_store_get_root(store), user, rel ? rel : "");
}

bool fs_upload(UserStore *store, const char *user, const char *dst_relpath, const char *tmp_src, size_t size, char *err, size_t errlen) {
	char dst[512];
	build_user_path(store, user, dst_relpath, dst, sizeof(dst));
	FILE *in = fopen(tmp_src, "rb");
	if (!in) { snprintf(err, errlen, "open tmp_src failed"); return false; }
	FILE *out = fopen(dst, "wb");
	if (!out) { fclose(in); snprintf(err, errlen, "open dst failed"); return false; }
	char buf[8192];
	size_t total = 0, n;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		fwrite(buf, 1, n, out);
		total += n;
	}
	fclose(in);
	fclose(out);
	(void)size; // could enforce quota elsewhere
	return true;
}

bool fs_download_path(UserStore *store, const char *user, const char *relpath, char *out_path, size_t out_len, char *err, size_t errlen) {
	(void)err; (void)errlen;
	build_user_path(store, user, relpath, out_path, out_len);
	return true;
}

bool fs_delete(UserStore *store, const char *user, const char *relpath, char *err, size_t errlen) {
	char path[512];
	build_user_path(store, user, relpath, path, sizeof(path));
	if (remove(path) != 0) { snprintf(err, errlen, "remove failed"); return false; }
	return true;
}

bool fs_list(UserStore *store, const char *user, char *buf, size_t buflen, char *err, size_t errlen) {
	char path[512];
	build_user_path(store, user, NULL, path, sizeof(path));
	DIR *d = opendir(path);
	if (!d) { snprintf(err, errlen, "opendir failed"); return false; }
	struct dirent *de;
	size_t used = 0;
	while ((de = readdir(d))) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
		int w = snprintf(buf + used, buflen - used, "%s\n", de->d_name);
		if (w < 0 || (size_t)w >= buflen - used) break;
		used += (size_t)w;
	}
	closedir(d);
	return true;
}


