#ifndef COMMON_H
#define COMMON_H

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/sshlike.sock"
#define MAX_BUF 4096
#define MAX_PASS 128
#define AUTH_OK 0
#define AUTH_FAIL 1

struct auth_req {
    char username[64];
    char password[128];
};

struct auth_resp {
    int status;
};

#endif
