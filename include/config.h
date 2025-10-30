#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

// Initialize configuration from file
void config_init(const char *config_file);

// Check if persistent storage should be used
bool config_use_persistent_storage(void);

// Get the storage path for persistent storage
const char *config_get_storage_path(void);

// Get the database filename for persistent storage
const char *config_get_database_file(void);

#endif // CONFIG_H
