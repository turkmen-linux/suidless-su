#include "pty.h"
#include "common.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>

int pty_allocate(int *master_fd, int *slave_fd, char *slave_name, size_t name_len) {
    int master, slave;
    char *pts_name;

    master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0) {
        perror("posix_openpt");
        return -1;
    }

    if (grantpt(master) < 0) {
        perror("grantpt");
        close(master);
        return -1;
    }

    if (unlockpt(master) < 0) {
        perror("unlockpt");
        close(master);
        return -1;
    }

    pts_name = ptsname(master);
    if (!pts_name) {
        perror("ptsname");
        close(master);
        return -1;
    }

    slave = open(pts_name, O_RDWR | O_NOCTTY);
    if (slave < 0) {
        perror("open slave");
        close(master);
        return -1;
    }

    if (name_len > 0) {
        strncpy(slave_name, pts_name, name_len - 1);
        slave_name[name_len - 1] = '\0';
    }

    *master_fd = master;
    *slave_fd = slave;
    return 0;
}

int pty_setup_terminal(int slave_fd) {
    struct termios term;
    if (tcgetattr(slave_fd, &term) < 0) {
        perror("tcgetattr");
        return -1;
    }
    cfmakeraw(&term);
    term.c_lflag |= (ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOKE);
    term.c_oflag |= OPOST;
    if (tcsetattr(slave_fd, TCSANOW, &term) < 0) {
        perror("tcsetattr");
        return -1;
    }
    return 0;
}

pid_t pty_fork_shell(int master_fd, int slave_fd, const char *slave_name) {
    pid_t pid;
    char *shell = getenv("SHELL");
    if (!shell) shell = "/bin/sh";

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        close(master_fd);
        setsid();

        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);

        if (ioctl(slave_fd, TIOCSCTTY, 0) < 0) {
            perror("ioctl TIOCSCTTY");
            exit(1);
        }

        close(slave_fd);
        execl(shell, shell, NULL);
        perror("execl");
        exit(1);
    }

    close(slave_fd);
    return pid;
}
