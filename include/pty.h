#ifndef PTY_H
#define PTY_H

#include <sys/types.h>
#include "common.h"

/* Allocate a PTY master/slave pair */
int pty_allocate(int *master_fd, int *slave_fd, char *slave_name, size_t name_len);

/* Configure terminal attributes for the PTY slave */
int pty_setup_terminal(int slave_fd);

#endif
