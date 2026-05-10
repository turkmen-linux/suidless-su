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

bool enable_pam;

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
    struct passwd *pw;
    pid_t shell_pid;
    ssize_t n;
    int master_fd, slave_fd;
    char slave_name[64];

    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0) {
        LOG("Socket: pid=%d uid=%d gid=%d\n", cred.pid, cred.uid, cred.gid);
    } else {
        perror("getsockopt");
    }
    req.cred = cred;
    req.client_fd = client_fd;


    // Check client is allowed
    unsigned long long ino_client, dev_client;
    unsigned long long ino_server, dev_server;
    char client_exe[PATH_MAX];
    sprintf(client_exe, "/proc/%d/exe", cred.pid);
    get_file_id(client_exe, &ino_client, &dev_client);
    get_file_id("/proc/self/exe", &ino_server, &dev_server);

    if(ino_client != ino_server || dev_client != dev_server){
        LOG("Illegal client %llu == %llu && %llu == %llu\n", ino_client, ino_server, dev_client, dev_server);
        return ;
    }


#ifdef PAM
    if(enable_pam){
        if(!pam_auth_socket(&req)){
            return;
        }
    } else {
#endif
        if(!crypt_auth_socket(&req)){
            return;
        }
#ifdef PAM
    }
#endif

    pw = getpwnam(req.auth.username);
    if (!pw) {
        puts(req.auth.username);
        return;
    }

    /* log command */
    if(req.session.command[0] == '\0'){
        LOG("Command (%s): SHELL\n", req.auth.username);
    } else {
        char *cmd_buf = req.session.command;
        char command[MAX_CMD] = "";
        while (*cmd_buf) {
            strcat(command, cmd_buf);
            strcat(command, " ");
            cmd_buf += strlen(cmd_buf)+1;
        }
        LOG("Command (%s): %s\n", req.auth.username, command);
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
