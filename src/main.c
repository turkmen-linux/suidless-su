#include "common.h"
#include "server.h"
#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <libgen.h>

int main(int argc, char *argv[]) {
    int daemon_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--daemon") == 0) {
            daemon_mode = 1;
            break;
        }
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
