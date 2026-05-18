#include "auth.h"
#include "common.h"

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include <sys/wait.h>

static int auth_validate(int client_fd, const char *username, const char *password);

static int sock_conv(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr) {
    int client_fd = *(int *)appdata_ptr;
    (void)client_fd;
    ssize_t n;
    const struct pam_message *msg_ptr = *msg;
    int x = 0;
    *resp = calloc(num_msg, sizeof(struct pam_response));
    for (x = 0; x < num_msg; x++, msg_ptr++) {
        struct client_request rreq;
        struct auth_resp rresp;
        switch (msg_ptr->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
        case PAM_PROMPT_ECHO_ON:
            rresp.status = AUTH_PROMPT;
            strcpy(rresp.prompt, msg_ptr->msg);
            LOG("PAM Prompt: %s\n", rresp.prompt);
            write(client_fd, &rresp, sizeof(rresp));
            n = read(client_fd, &rreq, sizeof(rreq));
            if (n != sizeof(rreq)) {
                LOG("Invalid request\n");
                return PAM_SERVICE_ERR;
            }
            resp[x]->resp = strdup(rreq.auth.password);
            puts(resp[x]->resp);
            break;

        case PAM_ERROR_MSG:
        case PAM_TEXT_INFO:
            rresp.status = AUTH_MSG;
            strcpy(rresp.prompt, msg_ptr->msg);
            LOG("PAM message: %s\n", rresp.prompt);
            write(client_fd, &rresp, sizeof(rresp));
            break;

        default:
            break;
        }
    }
    return PAM_SUCCESS;
}

bool pam_auth_socket(struct client_request *req) {
    struct client_request rreq;
    memset(&rreq, 0, sizeof(rreq));
    struct auth_resp rresp;
    ssize_t n;
    int authenticated = AUTH_FAIL;
    if (rreq.auth.username[0] == '\0') {
        n = read(req->client_fd, &rreq, sizeof(rreq));
        if (n != sizeof(rreq)) {
            LOG("Invalid request\n");
            return false;
        }
    }
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return false;
    }
    if(pid == 0){
        seteuid(req->cred.uid);
        int status = auth_validate(req->client_fd, rreq.auth.username, NULL);
        exit(status);
    } else {
        pid_t w = waitpid(pid, &authenticated, 0);
        if (w == -1) {
            perror("waitpid");
            return false;
        }
        if(authenticated > 1){
           authenticated = 1;
        }
    }
    if (authenticated == AUTH_OK) {
        LOG("Authentication success: %s\n", rreq.auth.username);
        rresp.status = AUTH_OK;
        write(req->client_fd, &rresp, sizeof(rresp));
        if (req)
            *req = rreq;
    } else if (authenticated == AUTH_FAIL) {
        LOG("Authentication fail: %s\n", rreq.auth.username);
        rresp.status = AUTH_FAIL;
        memset(&rreq.auth.password, 0, sizeof(rreq.auth.password));

        write(req->client_fd, &rresp, sizeof(rresp));

    } else {
        LOG("Unknown error: %s %d\n", rreq.auth.username, authenticated);
        return false;
    }
    return true;
}


static int auth_validate(int client_fd, const char *username, const char *password) {
    (void)password;
    int retval = AUTH_OK;
    pam_handle_t *pamh = NULL;
    struct pam_conv conv;
    conv.conv = sock_conv;
    conv.appdata_ptr = &client_fd;
    size_t auth_try = 0;

    while(auth_try < 3){

        LOG("Pam start\n");
        retval = pam_start("suidless-su", username, &conv, &pamh);
        if (retval != PAM_SUCCESS){
            retval = pam_start("su", username, &conv, &pamh);
        }
        if (retval != PAM_SUCCESS){
            LOG("Pam start fail\n");
            return AUTH_FAIL;
        }

        retval = pam_authenticate(pamh, 0);
        if (retval == PAM_SUCCESS){
            LOG("Pam acct mgmt fail\n");
            retval = pam_acct_mgmt(pamh, 0);
        }
        pam_end(pamh, retval);
        if(retval == PAM_SUCCESS){
            return AUTH_OK;
        }
    }
    return AUTH_FAIL;
}
