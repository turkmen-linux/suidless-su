#ifndef SERVER_H
#define SERVER_H

int server_init(void);
void server_run(int fd);
void server_cleanup(void);

#endif
