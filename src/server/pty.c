/*
 * pty.c - PTY (pseudo-terminal) allocation and setup
 *
 * Handles the creation and configuration of pseudo-terminal
 * pairs for use with shell processes.
 */

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

/*
 * Allocate a PTY master/slave pair.
 *
 * Opens a master PTY, grants access, unlocks it, and opens
 * the corresponding slave device. Returns the file descriptors
 * and the slave device name.
 */
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

/*
 * Configure the PTY slave terminal with raw mode settings
 * and standard terminal flags for interactive use.
 */
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
