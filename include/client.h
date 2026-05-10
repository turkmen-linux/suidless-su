#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"


int su_client_main(int argc, char *argv[]);
int sudo_client_main(int argc, char *argv[]);
int client_run(struct client_request req);
#endif
