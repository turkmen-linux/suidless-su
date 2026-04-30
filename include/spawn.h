#ifndef SPAWN_H
#define SPAWN_H

#include <sys/types.h>
#include <pwd.h>
#include "common.h"

/* Fork and spawn a shell in the PTY slave */
pid_t pty_fork_shell(int master_fd, int slave_fd, const char *slave_name,
                     struct session_req *session, struct passwd *pw);

#endif
