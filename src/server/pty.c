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

pid_t pty_fork_shell(int master_fd, int slave_fd, const char *slave_name, struct session_req *session, struct passwd *pw) {
    pid_t pid;
    char *shell;
    char login_shell[256];
    char *args[16];
    int arg_idx = 0;

    if (session->shell[0]) {
        shell = session->shell;
    } else if (pw && pw->pw_shell) {
        shell = pw->pw_shell;
    } else {
        shell = "/bin/sh";
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        close(master_fd);
        setgid(pw->pw_gid);
        setuid(pw->pw_uid);

        setsid();

        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);

        if (ioctl(slave_fd, TIOCSCTTY, 0) < 0) {
            perror("ioctl TIOCSCTTY");
            exit(1);
        }

        struct winsize ws;
        ws.ws_row = session->ws_row > 0 ? session->ws_row : 24;
        ws.ws_col = session->ws_col > 0 ? session->ws_col : 80;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
        ioctl(slave_fd, TIOCSWINSZ, &ws);

        close(slave_fd);

        if (session->login_flag) {
            clearenv();
            setenv("HOME", pw->pw_dir, 1);
            setenv("USER", pw->pw_name, 1);
            setenv("SHELL", shell, 1);
            setenv("TERM", "linux", 1);
            setenv("LOGNAME", pw->pw_name, 1);
            setenv("PATH", "/usr/local/bin:/bin:/usr/bin:/usr/local/sbin:/usr/sbin:/sbin", 1);
        } else if (session->env_vars[0] && !session->preserve_env) {
            extern char **environ;
            environ = NULL;
            char *env_buf = session->env_vars;
            while (*env_buf) {
                putenv(env_buf);
                env_buf += strlen(env_buf) + 1;
            }
        }

        if (session->login_flag) {
            char *base = strrchr(shell, '/');
            base = base ? base + 1 : shell;
            snprintf(login_shell, sizeof(login_shell), "-%.*s", (int)(sizeof(login_shell) - 2), base);
            args[arg_idx++] = login_shell;
            if (session->command[0]) {
                args[arg_idx++] = "-c";
                args[arg_idx++] = session->command;
            }
        } else {
            args[arg_idx++] = shell;
            if (session->command[0]) {
                args[arg_idx++] = "-c";
                args[arg_idx++] = session->command;
            }
        }

        args[arg_idx] = NULL;
        execv(shell, args);
        perror("execv");
        exit(1);
    }

    close(slave_fd);
    return pid;
}
