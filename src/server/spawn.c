/*
 * spawn.c - PTY shell spawning
 *
 * Handles forking and setting up a shell process in a PTY slave,
 * including user context switching, environment setup, and terminal
 * configuration.
 */

#include "spawn.h"
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
 * Fork a child process and spawn a shell in the PTY slave.
 *
 * Sets up user credentials, session, terminal window size,
 * environment variables, and executes the appropriate shell.
 * The child process becomes the shell, parent returns the PID.
 */
pid_t pty_fork_shell(int master_fd, int slave_fd, const char *slave_name,
                     struct session_req *session, struct passwd *pw) {
    pid_t pid;
    char *shell;
    char *args[16];
    int arg_idx = 0;

    /* Determine which shell to use: from session, passwd, or default */
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
        /* Child process: set up and exec the shell */

        /* Close master side in child */
        close(master_fd);

        /* Switch to target user */
        setgid(pw->pw_gid);
        setuid(pw->pw_uid);

        /* Create a new session and become session leader */
        setsid();

        /* Redirect stdio to the PTY slave */
        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);

        /* Set this PTY as the controlling terminal */
        if (ioctl(slave_fd, TIOCSCTTY, 0) < 0) {
            perror("ioctl TIOCSCTTY");
            exit(1);
        }

        /* Set terminal window size */
        struct winsize ws;
        ws.ws_row = session->ws_row > 0 ? session->ws_row : 24;
        ws.ws_col = session->ws_col > 0 ? session->ws_col : 80;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
        ioctl(slave_fd, TIOCSWINSZ, &ws);

        /* Close slave fd after duplication */
        close(slave_fd);

        /* Set up environment based on login flag */
        if (session->login_flag) {
            /* Login shell: clear env and set minimal login environment */
            char* term = getenv("TERM");
            clearenv();
            setenv("HOME", pw->pw_dir, 1);
            setenv("USER", pw->pw_name, 1);
            setenv("SHELL", shell, 1);
            if(term){
                setenv("TERM", term, 1);
            } else {
                setenv("TERM", "linux", 1);
            }
            setenv("LOGNAME", pw->pw_name, 1);
            setenv("PATH", "/usr/local/bin:/bin:/usr/bin:/usr/local/sbin:/usr/sbin:/sbin", 1);
            chdir(pw->pw_dir);
        } else if (session->env_vars[0] && !session->preserve_env) {
            chdir(session->path);
            /* Non-login: use provided environment variables */
            extern char **environ;
            environ = NULL;
            char *env_buf = session->env_vars;
            while (*env_buf) {
                putenv(env_buf);
                env_buf += strlen(env_buf) + 1;
            }
        }
        if (session->login_flag || session->command[0] == '\0') {
            args[arg_idx++] = shell;
        } else {
            char *cmd_buf = session->command;
            while (*cmd_buf) {
                args[arg_idx++] = cmd_buf;
                cmd_buf += strlen(cmd_buf) + 1;
            }
        }
        args[arg_idx] = NULL;
        
        execv(args[0], args);
        perror("execv");
        exit(1);
    }

    /* Parent process: close slave side */
    close(slave_fd);
    return pid;
}
