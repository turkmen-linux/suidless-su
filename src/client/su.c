#include <stdio.h>
#include <getopt.h>


#include "client.h"
#include "common.h"

int client_main(int argc, char *argv[]) {
    int opt;
    struct client_request req;
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
        {0, 0, 0, 0}
    };
    char *target_user = NULL;

    memset(&req, 0, sizeof(req));
    req.session.login_flag = 0;
    getcwd(req.session.path, sizeof(req.session.path));

    while ((opt = getopt_long(argc, argv, "mpg:G:lc:s:PTh", long_options, NULL)) != -1) {
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
                char* args[] = {"/bin/sh", "-c", optarg, NULL};
                char *cmd_buf = req.session.command;
                char *cmd_end = cmd_buf + sizeof(req.session.command);
                for (char **e = args; *e && cmd_buf < cmd_end - 1; e++) {
                    size_t len = strlen(*e);
                    if (cmd_buf + len + 1 < cmd_end) {
                        memcpy(cmd_buf, *e, len);
                        cmd_buf += len;
                        *cmd_buf++ = '\0';
                    }
                }
                *cmd_buf = '\0';
                break;
            case 's':
                strncpy(req.session.shell, optarg, MAX_SHELL - 1);
                break;
            case 'P':
                break;
            case 'T':
                break;
                return 0;
            case 'h':
            default:
                printf(
                    "Usage: su [options] [-] [username [args]]\n"
                    "\n"
                    "Options:\n"
                    "  -c, --command COMMAND         pass COMMAND to the invoked shell\n"
                    "  -m, -p,\n"
                    "  --preserve-environment        do not reset environment variables, and\n"
                    "                                keep the same shell\n"
                    "  -s, --shell SHELL             use SHELL instead of the default in passwd\n"
                    "  -h, --help                    display this help message and exit\n"
                    "If no username is given, assume root.\n"
                );
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
    return client_run(req);
}
