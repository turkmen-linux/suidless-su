#ifndef AUTH_H
#define AUTH_H

#include <sys/types.h>

int auth_validate(const char *username, const char *password);
int auth_check_user(const char *username);

#endif
