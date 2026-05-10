#include "auth.h"
#include "common.h"

#include <security/pam_appl.h>
#include <security/pam_misc.h>


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
            LOG("Prompt: %s\n", rresp.prompt);
            write(client_fd, &rresp, sizeof(rresp));
            n = read(client_fd, &rreq, sizeof(rreq));
            if (n != sizeof(rreq)) {
                LOG("Invalid request\n");
                return PAM_SERVICE_ERR;
            }
            resp[x]->resp = strdup(rreq.auth.password);
            break;

        case PAM_ERROR_MSG:
        case PAM_TEXT_INFO:
            printf("%s\n", msg_ptr->msg);
            break;

        default:
            break;
        }
    }
    return PAM_SUCCESS;
}

bool auth_socket(int client_fd, struct client_request *req) {
    struct client_request rreq;
    struct auth_resp rresp;
    ssize_t n;
    size_t auth_try = 1;
    int authenticated = AUTH_FAIL;
    while (1) {
        if (rreq.auth.username[0] == '\0') {
            n = read(client_fd, &rreq, sizeof(rreq));
            if (n != sizeof(rreq)) {
                LOG("Invalid request\n");
                return false;
            }
        }else {
            authenticated = auth_validate(client_fd, rreq.auth.username, NULL);
        }
        if (authenticated == AUTH_OK) {
            LOG("Authentication success: %s\n", rreq.auth.username);
            rresp.status = AUTH_OK;
            write(client_fd, &rresp, sizeof(rresp));
            if (req)
                *req = rreq;
            break;
        } else if (authenticated == AUTH_FAIL) {
            LOG("Authentication fail: %s try:%ld\n", rreq.auth.username, auth_try);
            rresp.status = AUTH_FAIL;
            write(client_fd, &rresp, sizeof(rresp));
            auth_try++;
            if(auth_try >= 3){
                return false;
            }
        }
    }
    return true;
}


int auth_validate(int client_fd, const char *username, const char *password) {
    (void)password;
    int retval = AUTH_OK;
    pam_handle_t *pamh = NULL;
    struct pam_conv conv;
    conv.conv = sock_conv;
    conv.appdata_ptr = &client_fd;

    retval = pam_start("suidless-su", username, &conv, &pamh);
    if (retval != PAM_SUCCESS){
        return AUTH_FAIL;
    }

    retval = pam_authenticate(pamh, 0);
    if (retval == PAM_SUCCESS){
        retval = pam_acct_mgmt(pamh, 0);
    }
    pam_end(pamh, retval);
    return retval == PAM_SUCCESS ? AUTH_OK : AUTH_FAIL;
}
