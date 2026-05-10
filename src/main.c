#include "common.h"
#include "server.h"
#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <libgen.h>

#ifdef PAM
extern bool enable_pam;
#endif
int main(int argc, char *argv[]) {
    int daemon_mode = 0;
#ifdef PAM
    enable_pam = true;
#endif
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--daemon") == 0) {
            daemon_mode = 1;
            continue;
        }
#ifdef PAM
        if (strcmp(argv[i], "--no-pam") == 0) {
            enable_pam = false;
            continue;
        }
#endif
    }

    if (daemon_mode) {
        if (geteuid() != 0) {
            fprintf(stderr, "Server must run as root\n");
            return 1;
        }
        return server_main(argc, argv);
    } else {
        optind = 1;
        char *name = basename(argv[0]);
        if(strcmp(name, "su") == 0){
            return su_client_main(argc, argv);
        } else if (strcmp(name, "sudo") == 0){
            return sudo_client_main(argc, argv);
        } else {
            printf("Command name is not allowed!\n");
            return 1;
        }
    }
}
