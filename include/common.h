#ifndef COMMON_H
#define COMMON_H

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>


#define SOCKET_PATH "/run/su.sock"
#define MAX_BUF 4096
#define MAX_PASS 128
#define MAX_CMD 1024
#define PATH_MAX 1024
#define MAX_SHELL 256
#define MAX_ENV 16384
#define AUTH_OK 0
#define AUTH_FAIL 1
#define AUTH_PROMPT 1

#define MSG_DATA  0x01
#define MSG_WINCH 0x02

struct auth_req {
    char username[64];
    char password[MAX_PASS];
};

struct auth_resp {
    int status;
    char prompt[MAX_BUF];
};

#define LOG(format, ...) do { \
    time_t now = time(NULL); \
    struct tm *timeinfo = localtime(&now); \
    char timestamp[26]; \
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo); \
    printf("[%s] " format, timestamp, ##__VA_ARGS__); \
} while(0)

struct session_req {
    int login_flag;
    char command[MAX_CMD];
    char path[PATH_MAX];
    char shell[MAX_SHELL];
    int preserve_env;
    char env_vars[MAX_ENV];
    int ws_row;
    int ws_col;
};

struct client_request {
    struct auth_req auth;
    struct session_req session;
};

#endif
