#ifndef AUTH_H
#define AUTH_H

#include "common.h"
#include <stdbool.h>

#include <sys/types.h>

bool pam_auth_socket(struct client_request *req);

bool crypt_auth_socket(struct client_request *req);

int get_file_id(const char *path, unsigned long long *out_ino, unsigned long long *out_dev);

#endif
