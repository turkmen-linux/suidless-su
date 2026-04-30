#include "server.h"
#include "auth.h"
#include "pty.h"
#include "common.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

static int server_fd = -1;

void server_cleanup(void) {
    if (server_fd >= 0) {
        close(server_fd);
    }
    unlink(SOCKET_PATH);
}

void handle_sigint(int sig) {
    server_cleanup();
    exit(0);
}

int server_init(void) {
    struct sockaddr_un addr;
    int fd;

    unlink(SOCKET_PATH);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 10) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    chmod(SOCKET_PATH, 0666);
    server_fd = fd;
    return fd;
}

void server_handle_client(int client_fd) {
    struct auth_req req;
    struct auth_resp resp;
    ssize_t n;
    pid_t shell_pid;
    int master_fd, slave_fd;
    char slave_name[64];

    n = read(client_fd, &req, sizeof(req));
    if (n != sizeof(req)) {
        return;
    }

    if (auth_validate(req.username, req.password) == AUTH_OK) {
        resp.status = AUTH_OK;
        write(client_fd, &resp, sizeof(resp));

        if (pty_allocate(&master_fd, &slave_fd, slave_name, sizeof(slave_name)) < 0) {
            return;
        }

        pty_setup_terminal(slave_fd);
        shell_pid = pty_fork_shell(master_fd, slave_fd, slave_name);

        if (shell_pid > 0) {
            char buf[MAX_BUF];
            fd_set fds;
            int maxfd;

            while (1) {
                FD_ZERO(&fds);
                FD_SET(client_fd, &fds);
                FD_SET(master_fd, &fds);
                maxfd = (client_fd > master_fd ? client_fd : master_fd) + 1;

                if (select(maxfd, &fds, NULL, NULL, NULL) < 0) {
                    break;
                }

                if (FD_ISSET(client_fd, &fds)) {
                    n = read(client_fd, buf, sizeof(buf));
                    if (n <= 0) break;
                    write(master_fd, buf, n);
                }

                if (FD_ISSET(master_fd, &fds)) {
                    n = read(master_fd, buf, sizeof(buf));
                    if (n <= 0) break;
                    write(client_fd, buf, n);
                }
            }

            kill(shell_pid, SIGTERM);
            waitpid(shell_pid, NULL, 0);
        }
        close(master_fd);
    } else {
        resp.status = AUTH_FAIL;
        write(client_fd, &resp, sizeof(resp));
    }

    close(client_fd);
}

void server_run(int fd) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    while (1) {
        int client_fd = accept(fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        if (fork() == 0) {
            close(fd);
            server_handle_client(client_fd);
            exit(0);
        }
        close(client_fd);
    }
}
