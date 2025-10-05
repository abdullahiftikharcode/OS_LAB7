#ifndef USER_H
#define USER_H

#include <pthread.h>
#include <stdbool.h>

typedef struct User {
	char name[64];
	char pass[64];
	pthread_mutex_t mutex; // per-user lock
	size_t used_bytes;
	size_t quota_bytes;
} User;

typedef struct UserNode {
	User user;
	struct UserNode *next;
} UserNode;

typedef struct UserStore {
	UserNode *head;
	pthread_mutex_t users_mutex;
	char storage_root[256];
	size_t user_count;
} UserStore;

// API functions
UserStore* user_store_create(const char *root);
void user_store_destroy(UserStore *store);
bool user_store_signup(UserStore *store, const char *name, const char *pass, size_t quota_bytes);
bool user_store_login(UserStore *store, const char *name, const char *pass);
User* user_store_lock_user(UserStore *store, const char *name);
void user_store_unlock_user(User *user);
const char* user_store_get_root(UserStore *store);

#endif

