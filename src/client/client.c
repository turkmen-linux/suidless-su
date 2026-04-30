#include "common.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/select.h>

static struct termios orig_termios;

void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

void enable_raw_mode(void) {
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &orig_termios) < 0) {
        perror("tcgetattr");
        exit(1);
    }
    atexit(disable_raw_mode);
    raw = orig_termios;
    cfmakeraw(&raw);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) {
        perror("tcsetattr");
        exit(1);
    }
}

void read_password(char *buf, size_t len) {
    struct termios old, new;
    if (tcgetattr(STDIN_FILENO, &old) < 0) {
        perror("tcgetattr");
        return;
    }
    new = old;
    new.c_lflag &= ~ECHO;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new) < 0) {
        perror("tcsetattr");
        return;
    }
    printf("Password: ");
    fflush(stdout);
    if (fgets(buf, len, stdin)) {
        buf[strcspn(buf, "\n")] = '\0';
    } else {
        buf[0] = '\0';
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    printf("\n");
}

void sigint_handler(int sig) {
    disable_raw_mode();
    exit(0);
}

int main(int argc, char *argv[]) {
    int fd;
    struct sockaddr_un addr;
    struct auth_req req;
    struct auth_resp resp;
    char buf[MAX_BUF];
    fd_set fds;
    int maxfd;
    ssize_t n;

    signal(SIGINT, sigint_handler);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    printf("Username: ");
    fflush(stdout);
    if (!fgets(req.username, sizeof(req.username), stdin)) {
        fprintf(stderr, "Failed to read username\n");
        close(fd);
        return 1;
    }
    req.username[strcspn(req.username, "\n")] = '\0';

    read_password(req.password, sizeof(req.password));

    if (write(fd, &req, sizeof(req)) != sizeof(req)) {
        perror("write auth req");
        close(fd);
        return 1;
    }

    if (read(fd, &resp, sizeof(resp)) != sizeof(resp)) {
        perror("read auth resp");
        close(fd);
        return 1;
    }

    if (resp.status != AUTH_OK) {
        fprintf(stderr, "Authentication failed\n");
        close(fd);
        return 1;
    }

    printf("Authenticated. Starting terminal...\n");
    enable_raw_mode();

    while (1) {
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(fd, &fds);
        maxfd = (STDIN_FILENO > fd ? STDIN_FILENO : fd) + 1;

        if (select(maxfd, &fds, NULL, NULL, NULL) < 0) {
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &fds)) {
            n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) break;
            write(fd, buf, n);
        }

        if (FD_ISSET(fd, &fds)) {
            n = read(fd, buf, sizeof(buf));
            if (n <= 0) break;
            write(STDIN_FILENO, buf, n);
        }
    }

    disable_raw_mode();
    close(fd);
    return 0;
}
