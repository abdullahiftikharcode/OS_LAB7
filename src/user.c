#include "user.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

// New struct-based API
UserStore* user_store_create(const char *root) {
	UserStore *store = (UserStore *)calloc(1, sizeof(UserStore));
	if (!store) return NULL;
	
	if (root && *root) {
		if (strlen(root) >= sizeof(store->storage_root)){
			return NULL;
		}
        strncpy(store->storage_root,root, sizeof(store->storage_root)-1);
		store->storage_root[sizeof(store->storage_root)-1] = '\0';
	}
	
	pthread_mutex_init(&store->users_mutex, NULL);
	mkdir(store->storage_root, 0777);
	return store;
}

void user_store_destroy(UserStore *store) {
	if (!store) return;
	
	pthread_mutex_lock(&store->users_mutex);
	UserNode *n = store->head;
	while (n) {
		UserNode *next = n->next;
		pthread_mutex_destroy(&n->user.mutex);
		free(n);
		n = next;
	}
	pthread_mutex_unlock(&store->users_mutex);
	pthread_mutex_destroy(&store->users_mutex);
	free(store);
}

bool user_store_signup(UserStore *store, const char *name, const char *pass, size_t quota_bytes) {
	pthread_mutex_lock(&store->users_mutex);
	
	// Check if user already exists
	UserNode *n = store->head;
	while (n) {
		if (strcmp(n->user.name, name) == 0) {
			pthread_mutex_unlock(&store->users_mutex);
			return false;
		}
		n = n->next;
	}
	
	// Create new user
	UserNode *node = (UserNode *)calloc(1, sizeof(UserNode));
	if (!node) {
		pthread_mutex_unlock(&store->users_mutex);
		return false;
	}
	
	strncpy(node->user.name, name, sizeof(node->user.name)-1);
	strncpy(node->user.pass, pass, sizeof(node->user.pass)-1);
	node->user.quota_bytes = quota_bytes;
	pthread_mutex_init(&node->user.mutex, NULL);
	
	node->next = store->head;
	store->head = node;
	store->user_count++;
	
	pthread_mutex_unlock(&store->users_mutex);
	
	// Create user directory
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", store->storage_root, name);
	mkdir(path, 0777);
	
	return true;
}

bool user_store_login(UserStore *store, const char *name, const char *pass) {
	pthread_mutex_lock(&store->users_mutex);
	
	UserNode *n = store->head;
	while (n) {
		if (strcmp(n->user.name, name) == 0) {
			bool ok = (strcmp(n->user.pass, pass) == 0);
			pthread_mutex_unlock(&store->users_mutex);
			return ok;
		}
		n = n->next;
	}
	
	pthread_mutex_unlock(&store->users_mutex);
	return false;
}

User* user_store_lock_user(UserStore *store, const char *name) {
	pthread_mutex_lock(&store->users_mutex);
	
	UserNode *n = store->head;
	while (n) {
		if (strcmp(n->user.name, name) == 0) {
			pthread_mutex_lock(&n->user.mutex);
			pthread_mutex_unlock(&store->users_mutex);
			return &n->user;
		}
		n = n->next;
	}
	
	pthread_mutex_unlock(&store->users_mutex);
	return NULL;
}

void user_store_unlock_user(User *user) {
	if (user) pthread_mutex_unlock(&user->mutex);
}

const char* user_store_get_root(UserStore *store) {
	return store ? store->storage_root : NULL;
}


