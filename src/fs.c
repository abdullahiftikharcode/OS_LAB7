#include "fs.h"
#include "crypto.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

static void build_user_path(UserStore *store, const char *user, const char *rel, char *out, size_t out_len) {
	snprintf(out, out_len, "%s/%s/%s", user_store_get_root(store), user, rel ? rel : "");
}

bool fs_upload(UserStore *store, const char *user, const char *dst_relpath, const char *tmp_src, size_t size, char *err, size_t errlen) {
    (void)size; // Size parameter not currently used but kept for API consistency
    if (!store || !user || !dst_relpath || !tmp_src) {
        snprintf(err, errlen, "Invalid parameters");
        return false;
    }
    
    char dst[512];
    build_user_path(store, user, dst_relpath, dst, sizeof(dst));
    
    // Create directory if it doesn't exist
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", user_store_get_root(store), user);
    if (mkdir(dir_path, 0755) == -1 && errno != EEXIST) {
        snprintf(err, errlen, "Failed to create user directory: %s", strerror(errno));
        return false;
    }
    
    FILE *in = fopen(tmp_src, "rb");
    if (!in) { 
        snprintf(err, errlen, "Failed to open source file: %s", strerror(errno)); 
        return false; 
    }
    
    FILE *out = fopen(dst, "wb");
    if (!out) { 
        fclose(in); 
        snprintf(err, errlen, "Failed to create destination file: %s", strerror(errno)); 
        return false; 
    }
    
    unsigned char in_buf[8192];
    unsigned char *encoded_buf = NULL;
    size_t total = 0, n;
    bool success = true;
    
    while ((n = fread(in_buf, 1, sizeof(in_buf), in)) > 0) {
        size_t encoded_len = 0;
        encoded_buf = encode_data(in_buf, n, &encoded_len);
        
        if (!encoded_buf) {
            success = false;
            snprintf(err, errlen, "Failed to encode data");
            break;
        }
        
        if (fwrite(encoded_buf, 1, encoded_len, out) != encoded_len) {
            success = false;
            snprintf(err, errlen, "Write failed: %s", strerror(errno));
            free(encoded_buf);
            break;
        }
        
        free(encoded_buf);
        encoded_buf = NULL;
        total += n;
    }
    
    // Cleanup in case of error
    if (encoded_buf) {
        free(encoded_buf);
    }
    
    // Check for read errors
    if (ferror(in)) {
        success = false;
        snprintf(err, errlen, "Read error: %s", strerror(errno));
    }
    
    // Close files
    fclose(in);
    
    if (fclose(out) != 0 && success) {
        success = false;
        snprintf(err, errlen, "Failed to close destination file: %s", strerror(errno));
    }
    
    // Remove destination file on error
    if (!success) {
        remove(dst);
    }
    
    return success;
}

bool fs_download_path(UserStore *store, const char *user, const char *relpath, char *out_path, size_t out_len, char *err, size_t errlen) {
    if (!store || !user || !relpath || !out_path || !err) {
        if (err && errlen > 0) {
            snprintf(err, errlen, "Invalid parameters");
        }
        return false;
    }
    
    char path[512];
    build_user_path(store, user, relpath, path, sizeof(path));
    
    // Check if file exists and is readable
    FILE *file = fopen(path, "rb");
    if (!file) {
        snprintf(err, errlen, "File not found or inaccessible: %s", strerror(errno));
        return false;
    }
    
    // Get file size for progress reporting (optional)
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        snprintf(err, errlen, "Failed to determine file size: %s", strerror(errno));
        return false;
    }
    
    long file_size = ftell(file);
    if (file_size == -1) {
        fclose(file);
        snprintf(err, errlen, "Failed to get file size: %s", strerror(errno));
        return false;
    }
    
    rewind(file);
    
    // Create temp directory if it doesn't exist
    const char *temp_dir = "/tmp/os_lab7_downloads";
    if (mkdir(temp_dir, 0755) == -1 && errno != EEXIST) {
        fclose(file);
        snprintf(err, errlen, "Failed to create temp directory: %s", strerror(errno));
        return false;
    }
    
    // Create temp file with a unique name
    char temp_path[512];
    time_t now = time(NULL);
    snprintf(temp_path, sizeof(temp_path), "%s/%s_%s_%ld.tmp", 
             temp_dir, user, relpath, (long)now);
    
    // Ensure the filename is valid by replacing any problematic characters
    for (char *p = temp_path; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' || 
            *p == '"' || *p == '<' || *p == '>' || *p == '|') {
            *p = '_';
        }
    }
    
    FILE *temp_file = fopen(temp_path, "wb");
    if (!temp_file) {
        fclose(file);
        snprintf(err, errlen, "Failed to create temporary file: %s", strerror(errno));
        return false;
    }
    
    bool success = true;
    unsigned char in_buf[8192];
    unsigned char *decoded_buf = NULL;
    
    // Read, decode and write file data
    while (1) {
        size_t bytes_read = fread(in_buf, 1, sizeof(in_buf), file);
        
        if (bytes_read == 0) {
            // Check for read errors
            if (ferror(file)) {
                success = false;
                snprintf(err, errlen, "Error reading from file: %s", strerror(errno));
            }
            break;
        }
        
        // Decode the data
        size_t decoded_len = 0;
        decoded_buf = decode_data(in_buf, bytes_read, &decoded_len);
        
        if (!decoded_buf) {
            success = false;
            snprintf(err, errlen, "Failed to decode data");
            break;
        }
        
        // Write the decoded data to the temp file
        if (fwrite(decoded_buf, 1, decoded_len, temp_file) != decoded_len) {
            success = false;
            snprintf(err, errlen, "Failed to write to temporary file: %s", strerror(errno));
            free(decoded_buf);
            break;
        }
        
        free(decoded_buf);
        decoded_buf = NULL;
    }
    
    // Cleanup
    if (decoded_buf) {
        free(decoded_buf);
    }
    
    // Close files
    if (fclose(temp_file) != 0 && success) {
        success = false;
        snprintf(err, errlen, "Error closing temporary file: %s", strerror(errno));
    }
    
    fclose(file);
    
    // If there was an error, clean up the temp file
    if (!success) {
        remove(temp_path);
        return false;
    }
    
    // Ensure the temp file is readable by the client
    chmod(temp_path, 0644);
    
    // Copy the temp file path to the output buffer
    size_t path_len = strlen(temp_path);
    if (path_len >= out_len) {
        remove(temp_path);
        snprintf(err, errlen, "Temporary file path is too long");
        return false;
    }
    
    strncpy(out_path, temp_path, out_len - 1);
    out_path[out_len - 1] = '\0';
    
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


