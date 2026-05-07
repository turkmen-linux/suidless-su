#include "server.h"
#include "auth.h"
#include "pty.h"
#include "spawn.h"
#include "common.h"
#include "client.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/select.h>

#define LOG(format, ...) do { \
    time_t now = time(NULL); \
    struct tm *timeinfo = localtime(&now); \
    char timestamp[26]; \
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo); \
    printf("[%s] " format, timestamp, ##__VA_ARGS__); \
} while(0)


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
    struct client_request req;
    struct auth_resp resp;
    ssize_t n;
    pid_t shell_pid;
    int master_fd, slave_fd;
    char slave_name[64];
    struct passwd *pw;
    size_t auth_delay = 0;

    while(1){
        n = read(client_fd, &req, sizeof(req));
        if (n != sizeof(req)) {
            return;
        }
        if (auth_validate(req.auth.username, req.auth.password) == AUTH_OK) {
            LOG("Authentication success: %s\n", req.auth.username);
            usleep(auth_delay*1000);
            resp.status = AUTH_OK;
            write(client_fd, &resp, sizeof(resp));
            break;
        } else {
            auth_delay+= 1000;
            usleep(auth_delay*1000);
            LOG("Authentication fail: %s\n", req.auth.username);
            resp.status = AUTH_FAIL;
            write(client_fd, &resp, sizeof(resp));
        }
    }

    pw = getpwnam(req.auth.username);
    if (!pw) {
        return;
    }

    if (pty_allocate(&master_fd, &slave_fd, slave_name, sizeof(slave_name)) < 0) {
        return;
    }

    pty_setup_terminal(slave_fd);
    shell_pid = pty_fork_shell(master_fd, slave_fd, slave_name, &req.session, pw);

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
                char type;
                if (read(client_fd, &type, 1) != 1) break;

                if (type == MSG_DATA) {
                    n = read(client_fd, buf, sizeof(buf));
                    if (n <= 0) break;
                    write(master_fd, buf, n);
                } else if (type == MSG_WINCH) {
                    struct winsize ws;
                    ssize_t r = read(client_fd, (char*)&ws, sizeof(ws));
                    if (r <= 0) break;
                    ioctl(master_fd, TIOCSWINSZ, &ws);
                }
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

int server_main(int argc, char *argv[]) {
    int fd;

    fd = server_init();
    if (fd < 0) {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }

    server_run(fd);
    server_cleanup();

    return 0;
}
