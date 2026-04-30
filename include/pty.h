#ifndef PTY_H
#define PTY_H

#include <sys/types.h>

int pty_allocate(int *master_fd, int *slave_fd, char *slave_name, size_t name_len);
int pty_setup_terminal(int slave_fd);
pid_t pty_fork_shell(int master_fd, int slave_fd, const char *slave_name);

#endif
