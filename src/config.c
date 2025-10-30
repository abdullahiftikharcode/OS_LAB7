#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#define MAX_LINE_LENGTH 256
#define MAX_PATH_LENGTH 512

// Default configuration
static struct {
    bool use_persistent_storage;
    char storage_path[MAX_PATH_LENGTH];
    char database_file[MAX_PATH_LENGTH];
} config;

// Forward declarations
static void trim_whitespace(char *str);
static void set_default_config(void);

void config_init(const char *config_file) {
    // Set default values first
    set_default_config();
    
    // Try to open the config file
    FILE *file = fopen(config_file, "r");
    if (!file) {
        fprintf(stderr, "Warning: Could not open config file '%s', using defaults\n", config_file);
        return;
    }
    
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == '\0') {
            continue;
        }
        
        // Remove newline and carriage return
        line[strcspn(line, "\r\n")] = '\0';
        
        // Split into key and value
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");
        
        if (key && value) {
            // Trim whitespace
            trim_whitespace(key);
            trim_whitespace(value);
            
            // Process configuration options
            if (strcmp(key, "use_persistent_storage") == 0) {
                config.use_persistent_storage = (strcmp(value, "1") == 0);
            } 
            else if (strcmp(key, "storage_path") == 0) {
                strncpy(config.storage_path, value, sizeof(config.storage_path) - 1);
                config.storage_path[sizeof(config.storage_path) - 1] = '\0';
            }
            else if (strcmp(key, "database_file") == 0) {
                strncpy(config.database_file, value, sizeof(config.database_file) - 1);
                config.database_file[sizeof(config.database_file) - 1] = '\0';
            }
        }
    }
    
    fclose(file);
    
    // Create storage directory if it doesn't exist and we're using persistent storage
    if (config.use_persistent_storage) {
        struct stat st = {0};
        if (stat(config.storage_path, &st) == -1) {
            mkdir(config.storage_path, 0777);
        }
    }
}

bool config_use_persistent_storage(void) {
    return config.use_persistent_storage;
}

const char *config_get_storage_path(void) {
    return config.storage_path;
}

const char *config_get_database_file(void) {
    return config.database_file;
}

// Helper function to trim whitespace from a string
static void trim_whitespace(char *str) {
    if (!str) return;
    
    // Trim leading space
    char *start = str;
    while (*start && (*start == ' ' || *start == '\t')) {
        start++;
    }
    
    // All spaces?
    if (*start == '\0') {
        *str = '\0';
        return;
    }
    
    // Trim trailing space
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    
    // Write new null terminator
    *(end + 1) = '\0';
    
    // Move the string to the start if needed
    if (start != str) {
        memmove(str, start, (end - start) + 2); // +2 for null terminator
    }
}

// Set default configuration values
static void set_default_config(void) {
    config.use_persistent_storage = true; // Default to persistent storage
    strncpy(config.storage_path, "./storage", sizeof(config.storage_path));
    strncpy(config.database_file, "users.db", sizeof(config.database_file));
}
