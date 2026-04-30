#include "common.h"
#include "server.h"
#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

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
        return client_main(argc, argv);
    }
}
