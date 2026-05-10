#ifndef AUTH_H
#define AUTH_H

#include "common.h"

#include <sys/types.h>

bool auth_socket(int client_fd, struct client_request *req);
int auth_validate(const char *username, const char *password);
int auth_check_user(const char *username);

int get_file_id(const char *path, unsigned long long *out_ino, unsigned long long *out_dev);

#endif
