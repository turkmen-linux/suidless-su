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
#define MAX_CMD 1024
#define MAX_SHELL 256
#define AUTH_OK 0
#define AUTH_FAIL 1

struct auth_req {
    char username[64];
    char password[128];
};

struct auth_resp {
    int status;
};

struct session_req {
    int login_flag;
    char command[MAX_CMD];
    char shell[MAX_SHELL];
    int preserve_env;
};

struct client_request {
    struct auth_req auth;
    struct session_req session;
};

#endif
