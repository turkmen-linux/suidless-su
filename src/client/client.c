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
#include <pwd.h>
#include <ctype.h>

static struct termios orig_termios;
static int winch_pipe[2] = {-1, -1};

void sigwinch_handler(int sig) {
    (void)sig;
    write(winch_pipe[1], "", 1);
}

static void send_winch(int fd) {
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) != 0) return;
    char type = MSG_WINCH;
    write(fd, &type, 1);
    write(fd, &ws, sizeof(ws));
}

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

    if (fgets(buf, len, stdin)) {
        buf[strcspn(buf, "\n")] = '\0';
    } else {
        buf[0] = '\0';
        clearerr(stdin);
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    printf("\n");
}

void sigint_handler(int sig) {
    (void)sig;
    disable_raw_mode();
    exit(0);
}

int client_run(struct client_request req){
    int fd;
    struct sockaddr_un addr;
    struct auth_resp resp;
    char buf[MAX_BUF];
    fd_set fds;
    int maxfd;
    ssize_t n;


    signal(SIGINT, sigint_handler);
    signal(SIGWINCH, sigwinch_handler);
    pipe(winch_pipe);

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
    if (write(fd, &req, sizeof(req)) != sizeof(req)) {
        perror("write request");
        close(fd);
        return 1;
    }

    while(1){

        if (read(fd, &resp, sizeof(resp)) != sizeof(resp)) {
            perror("read auth resp");
            close(fd);
            return 1;
        }
        
        if(resp.status == AUTH_MSG) {
            printf(resp.prompt);
            fflush(stdout);
            continue;
        } else if(resp.status == AUTH_PROMPT) {
            printf(resp.prompt);
            fflush(stdout);
            read_password(req.auth.password, sizeof(req.auth.password));
            if (write(fd, &req, sizeof(req)) != sizeof(req)) {
                perror("write request");
                close(fd);
                return 1;
            }
            continue;
        }else if (resp.status == AUTH_OK) {
            break;
        } else if (resp.status == AUTH_FAIL){
            fprintf(stderr, "Authentication failed\n");
            continue;
        }
    }

    enable_raw_mode();

    while (1) {
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(fd, &fds);
        FD_SET(winch_pipe[0], &fds);
        maxfd = fd;
        if (STDIN_FILENO > maxfd) maxfd = STDIN_FILENO;
        if (winch_pipe[0] > maxfd) maxfd = winch_pipe[0];
        maxfd++;

        if (select(maxfd, &fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (FD_ISSET(winch_pipe[0], &fds)) {
            char c;
            read(winch_pipe[0], &c, 1);
            send_winch(fd);
        }

        if (FD_ISSET(STDIN_FILENO, &fds)) {
            n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) break;
            char type = MSG_DATA;
            write(fd, &type, 1);
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


