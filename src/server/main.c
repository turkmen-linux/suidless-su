#include "server.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {
    int fd;

    if (geteuid() != 0) {
        fprintf(stderr, "Server must run as root\n");
        return 1;
    }

    fd = server_init();
    if (fd < 0) {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }

    printf("Server listening on %s\n", SOCKET_PATH);
    server_run(fd);
    server_cleanup();

    return 0;
}
