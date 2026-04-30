#include "auth.h"
#include "common.h"
#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>
#include <crypt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int auth_validate(const char *username, const char *password) {
    struct spwd *sp;
    char *encrypted;

    sp = getspnam(username);
    if (!sp) {
        return AUTH_FAIL;
    }

    encrypted = crypt(password, sp->sp_pwdp);
    if (!encrypted) {
        return AUTH_FAIL;
    }

    if (strcmp(encrypted, sp->sp_pwdp) == 0) {
        return AUTH_OK;
    }

    return AUTH_FAIL;
}

int auth_check_user(const char *username) {
    struct passwd *pw = getpwnam(username);
    return pw ? 0 : -1;
}
