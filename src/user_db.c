#include "user.h"
#include "config.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

// In-memory user storage for non-persistent mode
typedef struct InMemoryUser {
    char name[64];
    char pass[64];
    size_t quota_bytes;
    struct InMemoryUser *next;
} InMemoryUser;

// User store implementation
typedef enum {
    STORAGE_PERSISTENT,
    STORAGE_IN_MEMORY
} StorageType;

// SQL statements
#define CREATE_TABLE_SQL \
    "CREATE TABLE IF NOT EXISTS users (\n" \
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n" \
    "  username TEXT UNIQUE NOT NULL,\n" \
    "  password TEXT NOT NULL,\n" \
    "  quota_bytes INTEGER DEFAULT 104857600,\n" \
    "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP\n" \
    ");"

// Complete definition of UserStore (declared as opaque in user.h)
struct UserStore {
    char storage_root[256];
    StorageType storage_type;
    union {
        struct {
            sqlite3 *db;
            pthread_mutex_t db_mutex;
        } persistent;
        struct {
            InMemoryUser *users;
            pthread_mutex_t users_mutex;
        } memory;
    } storage;
};

// Forward declarations
static int execute_sql(sqlite3 *db, const char *sql, int (*callback)(void*,int,char**,char**), void *arg);

UserStore* user_store_create(const char *root) {
    UserStore *store = (UserStore *)calloc(1, sizeof(UserStore));
    if (!store) return NULL;
    
    // Initialize based on configuration
    store->storage_type = config_use_persistent_storage() ? STORAGE_PERSISTENT : STORAGE_IN_MEMORY;
    
    // Set storage root
    if (root && *root) {
        strncpy(store->storage_root, root, sizeof(store->storage_root)-1);
        store->storage_root[sizeof(store->storage_root)-1] = '\0';
        mkdir(store->storage_root, 0777);
    }
    
    if (store->storage_type == STORAGE_PERSISTENT) {
        // Initialize SQLite database for persistent storage
        char db_path[512];
        snprintf(db_path, sizeof(db_path), "%s/%s", 
                store->storage_root, config_get_database_file());
        
        if (sqlite3_open(db_path, &store->storage.persistent.db) != SQLITE_OK) {
            fprintf(stderr, "Failed to open database: %s\n", 
                   sqlite3_errmsg(store->storage.persistent.db));
            free(store);
            return NULL;
        }
        
        // Enable foreign keys
        execute_sql(store->storage.persistent.db, "PRAGMA foreign_keys = ON;", NULL, NULL);
        
        // Create users table if it doesn't exist
        if (execute_sql(store->storage.persistent.db, CREATE_TABLE_SQL, NULL, NULL) != SQLITE_OK) {
            fprintf(stderr, "Failed to create users table: %s\n", 
                   sqlite3_errmsg(store->storage.persistent.db));
            sqlite3_close(store->storage.persistent.db);
            free(store);
            return NULL;
        }
        
        pthread_mutex_init(&store->storage.persistent.db_mutex, NULL);
    } else {
        // Initialize in-memory storage
        store->storage.memory.users = NULL;
        pthread_mutex_init(&store->storage.memory.users_mutex, NULL);
    }
    
    return store;
}

void user_store_destroy(UserStore *store) {
    if (!store) return;
    
    if (store->storage_type == STORAGE_PERSISTENT) {
        pthread_mutex_destroy(&store->storage.persistent.db_mutex);
        if (store->storage.persistent.db) {
            sqlite3_close(store->storage.persistent.db);
        }
    } else {
        // Free all in-memory users
        pthread_mutex_lock(&store->storage.memory.users_mutex);
        InMemoryUser *user = store->storage.memory.users;
        while (user) {
            InMemoryUser *next = user->next;
            free(user);
            user = next;
        }
        pthread_mutex_unlock(&store->storage.memory.users_mutex);
        pthread_mutex_destroy(&store->storage.memory.users_mutex);
    }
    
    free(store);
}

bool user_store_signup(UserStore *store, const char *name, const char *pass, size_t quota_bytes, TaskPriority priority) {
    if (!store || !name || !pass) return false;
    (void)priority; // Priority is stored in the User struct after creation
    
    if (store->storage_type == STORAGE_PERSISTENT) {
        // Persistent storage using SQLite
        int rc;
        
        pthread_mutex_lock(&store->storage.persistent.db_mutex);
        
        // Check if user already exists
        sqlite3_stmt *stmt;
        const char *check_sql = "SELECT 1 FROM users WHERE username = ?;";
        
        rc = sqlite3_prepare_v2(store->storage.persistent.db, check_sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            pthread_mutex_unlock(&store->storage.persistent.db_mutex);
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_ROW) {
            // User already exists
            pthread_mutex_unlock(&store->storage.persistent.db_mutex);
            return false;
        }
        
        // Insert new user
        const char *insert_sql = "INSERT INTO users (username, password, quota_bytes) VALUES (?, ?, ?);";
        rc = sqlite3_prepare_v2(store->storage.persistent.db, insert_sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            pthread_mutex_unlock(&store->storage.persistent.db_mutex);
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, pass, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, quota_bytes);
        
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        bool success = (rc == SQLITE_DONE);
        pthread_mutex_unlock(&store->storage.persistent.db_mutex);
        
        if (success) {
            // Create user directory
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", store->storage_root, name);
            mkdir(path, 0777);
        }
        
        return success;
    } else {
        // In-memory storage
        pthread_mutex_lock(&store->storage.memory.users_mutex);
        
        // Check if user already exists
        InMemoryUser *user = store->storage.memory.users;
        while (user) {
            if (strcmp(user->name, name) == 0) {
                pthread_mutex_unlock(&store->storage.memory.users_mutex);
                return false;
            }
            user = user->next;
        }
        
        // Create new user
        InMemoryUser *new_user = (InMemoryUser *)calloc(1, sizeof(InMemoryUser));
        if (!new_user) {
            pthread_mutex_unlock(&store->storage.memory.users_mutex);
            return false;
        }
        
        strncpy(new_user->name, name, sizeof(new_user->name) - 1);
        strncpy(new_user->pass, pass, sizeof(new_user->pass) - 1);
        new_user->quota_bytes = quota_bytes;
        
        // Add to the beginning of the list
        new_user->next = store->storage.memory.users;
        store->storage.memory.users = new_user;
        
        pthread_mutex_unlock(&store->storage.memory.users_mutex);
        
        // Create user directory
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", store->storage_root, name);
        mkdir(path, 0777);
        
        return true;
    }
}

bool user_store_login(UserStore *store, const char *name, const char *pass) {
    if (!store || !name || !pass) return false;
    
    if (store->storage_type == STORAGE_PERSISTENT) {
        // Persistent storage using SQLite
        pthread_mutex_lock(&store->storage.persistent.db_mutex);
        
        sqlite3_stmt *stmt;
        const char *sql = "SELECT password FROM users WHERE username = ?;";
        bool authenticated = false;
        
        if (sqlite3_prepare_v2(store->storage.persistent.db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
            
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *stored_pass = (const char *)sqlite3_column_text(stmt, 0);
                if (stored_pass && strcmp(stored_pass, pass) == 0) {
                    authenticated = true;
                }
            }
            
            sqlite3_finalize(stmt);
        }
        
        pthread_mutex_unlock(&store->storage.persistent.db_mutex);
        return authenticated;
    } else {
        // In-memory storage
        pthread_mutex_lock(&store->storage.memory.users_mutex);
        
        InMemoryUser *user = store->storage.memory.users;
        bool authenticated = false;
        
        while (user) {
            if (strcmp(user->name, name) == 0) {
                if (strcmp(user->pass, pass) == 0) {
                    authenticated = true;
                }
                break;
            }
            user = user->next;
        }
        
        pthread_mutex_unlock(&store->storage.memory.users_mutex);
        return authenticated;
    }
}

User* user_store_lock_user(UserStore *store, const char *name) {
    if (!store || !name) return NULL;
    
    User *user = (User *)calloc(1, sizeof(User));
    if (!user) return NULL;
    
    if (store->storage_type == STORAGE_PERSISTENT) {
        // Persistent storage using SQLite
        pthread_mutex_lock(&store->storage.persistent.db_mutex);
        
        sqlite3_stmt *stmt;
        const char *sql = "SELECT username, quota_bytes FROM users WHERE username = ?;";
        
        if (sqlite3_prepare_v2(store->storage.persistent.db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
            
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *username = (const char *)sqlite3_column_text(stmt, 0);
                if (username) {
                    strncpy(user->name, username, sizeof(user->name) - 1);
                    user->quota_bytes = sqlite3_column_int64(stmt, 1);
                    pthread_mutex_init(&user->mutex, NULL);
                    sqlite3_finalize(stmt);
                    pthread_mutex_unlock(&store->storage.persistent.db_mutex);
                    return user;
                }
            }
            
            sqlite3_finalize(stmt);
        }
        
        pthread_mutex_unlock(&store->storage.persistent.db_mutex);
        free(user);
        return NULL;
    } else {
        // In-memory storage
        pthread_mutex_lock(&store->storage.memory.users_mutex);
        
        InMemoryUser *mem_user = store->storage.memory.users;
        while (mem_user) {
            if (strcmp(mem_user->name, name) == 0) {
                strncpy(user->name, mem_user->name, sizeof(user->name) - 1);
                user->name[sizeof(user->name) - 1] = '\0';
                user->quota_bytes = mem_user->quota_bytes;
                pthread_mutex_init(&user->mutex, NULL);
                pthread_mutex_unlock(&store->storage.memory.users_mutex);
                return user;
            }
            mem_user = mem_user->next;
        }
        
        pthread_mutex_unlock(&store->storage.memory.users_mutex);
        free(user);
        return NULL;
    }
}

void user_store_unlock_user(User *user) {
    if (!user) return;
    pthread_mutex_destroy(&user->mutex);
    free(user);
}

const char* user_store_get_root(UserStore *store) {
    return store ? store->storage_root : NULL;
}

// Helper function to execute SQL statements
static int execute_sql(sqlite3 *db, const char *sql, int (*callback)(void*,int,char**,char**), void *arg) {
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, callback, arg, &err_msg);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
    
    return rc;
}
