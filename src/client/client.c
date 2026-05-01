#include "client.h"
#include "common.h"
#include "server.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <pwd.h>
#include <ctype.h>

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

int client_main(int argc, char *argv[]) {
    int fd;
    struct sockaddr_un addr;
    struct client_request req;
    struct auth_resp resp;
    char buf[MAX_BUF];
    fd_set fds;
    int maxfd;
    ssize_t n;
    int opt;
    struct option long_options[] = {
        {"preserve-environment", no_argument, 0, 'm'},
        {"group", required_argument, 0, 'g'},
        {"supp-group", required_argument, 0, 'G'},
        {"login", no_argument, 0, 'l'},
        {"command", required_argument, 0, 'c'},
        {"shell", required_argument, 0, 's'},
        {"pty", no_argument, 0, 'P'},
        {"no-pty", no_argument, 0, 'T'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {0, 0, 0, 0}
    };
    char *target_user = NULL;

    memset(&req, 0, sizeof(req));
    req.session.login_flag = 0;

    while ((opt = getopt_long(argc, argv, "mpg:G:lc:s:PTVh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'm':
            case 'p':
                req.session.preserve_env = 1;
                break;
            case 'g':
                break;
            case 'G':
                break;
            case 'l':
            case '-':
                req.session.login_flag = 1;
                break;
            case 'c':
                strncpy(req.session.command, optarg, MAX_CMD - 1);
                break;
            case 's':
                strncpy(req.session.shell, optarg, MAX_SHELL - 1);
                break;
            case 'P':
                break;
            case 'T':
                break;
        case 'V':
            printf("sshlike 1.0\n");
            return 0;
        case 'h':
        default:
            printf("Usage: %s [options] [-] [<user>]\n", argv[0]);
            printf("Use --help for full options\n");
            return 0;
        }
    }

    if (optind < argc) {
        if (strcmp(argv[optind], "-") == 0) {
            req.session.login_flag = 1;
            optind++;
        }
        if (optind < argc) {
            target_user = argv[optind];
            strncpy(req.auth.username, target_user, sizeof(req.auth.username) - 1);
        }
    }

    if (req.auth.username[0] == '\0') {
        strncpy(req.auth.username, "root", sizeof(req.auth.username) - 1);
    }

    signal(SIGINT, sigint_handler);

    if (!req.session.preserve_env) {
        char *env_buf = req.session.env_vars;
        char *env_end = env_buf + sizeof(req.session.env_vars);
        extern char **environ;
        for (char **e = environ; *e && env_buf < env_end - 1; e++) {
            size_t len = strlen(*e);
            if (env_buf + len + 1 < env_end) {
                memcpy(env_buf, *e, len);
                env_buf += len;
                *env_buf++ = '\0';
            }
        }
        *env_buf = '\0';
    }

    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
        req.session.ws_row = ws.ws_row;
        req.session.ws_col = ws.ws_col;
    } else {
        req.session.ws_row = 24;
        req.session.ws_col = 80;
    }

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

    read_password(req.auth.password, sizeof(req.auth.password));

    if (write(fd, &req, sizeof(req)) != sizeof(req)) {
        perror("write request");
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
