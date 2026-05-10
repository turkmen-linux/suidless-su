#include "auth.h"
#include "common.h"
#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>
#include <crypt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

bool auth_socket(int client_fd, struct client_request *req) {
    ssize_t n;
    size_t auth_delay = 0;
    struct client_request rreq;
    
    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0) {
        LOG("Socket: pid=%d uid=%d gid=%d\n", cred.pid, cred.uid, cred.gid);
    } else {
        perror("getsockopt");
    }


    size_t auth_try = 1;
    int authenticated = AUTH_FAIL;
    while(1){
        struct auth_resp resp;
        n = read(client_fd, &rreq, sizeof(rreq));
        if (n != sizeof(rreq)) {
            LOG("invalid request\n");
            return false;
        }
        if(cred.uid == 0){
            resp.status = AUTH_OK;
            write(client_fd, &resp, sizeof(resp));
            break;
        }
        //printf("%d %s %s\n",authenticated,  rreq.auth.username, rreq.auth.password);
        if(rreq.auth.password[0] == '\0' && authenticated != AUTH_OK){
            resp.status = AUTH_PROMPT;
            strcpy(resp.prompt, "Password: ");
            LOG("Prompt: %s\n", resp.prompt);
            write(client_fd, &resp, sizeof(resp));
            continue;
        }
        authenticated = auth_validate(rreq.auth.username, rreq.auth.password);
        if (authenticated == AUTH_OK) {
            LOG("Authentication success: %s\n", rreq.auth.username);
            usleep(auth_delay*1000);
            resp.status = AUTH_OK;
            write(client_fd, &resp, sizeof(resp));
            break;
        } else {
            auth_delay+= 500;
            usleep(auth_delay*1000);
            LOG("Authentication fail: %s try:%ld\n", rreq.auth.username, auth_try);
            resp.status = AUTH_FAIL;
            write(client_fd, &resp, sizeof(resp));
            auth_try++;
            if(auth_try >= 3){
                return false;
            }
        }
    }
    *req = rreq;
    return true;
}

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
