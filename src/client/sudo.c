#include <stdio.h>
#include <getopt.h>


#include "client.h"
#include "common.h"

int sudo_client_main(int argc, char *argv[]) {
    struct client_request req;
    /* Request struct init */
    memset(&req, 0, sizeof(req));
    req.session.login_flag = 0;
    getcwd(req.session.path, sizeof(req.session.path));
    strncpy(req.auth.username, "root", sizeof(req.auth.username) - 1);

    /* Copy argv */
    char *cmd_buf = req.session.command;
    char *cmd_end = cmd_buf + sizeof(req.session.command);
    if(argc < 2){
        printf("Usage: sudo [command]\n");
        return 2;
    }
    for (int i=1; i < argc && cmd_buf < cmd_end - 1; i++) {
        char* e = argv[i];
        size_t len = strlen(e);
        if (cmd_buf + len + 1 < cmd_end) {
            memcpy(cmd_buf, e, len);
            cmd_buf += len;
            *cmd_buf++ = '\0';
        }
    }
    *cmd_buf = '\0';
    return client_run(req);
}
