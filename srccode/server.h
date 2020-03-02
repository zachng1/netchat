#ifndef SERVER
#define SERVER

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <stropts.h>

struct clientinfo {
    int fd;
    unsigned int secretkey;
    char name[128];
};
int send_fd_pipe(int pipe, int sendfd, struct msghdr * msgh);
struct msghdr * recv_fd_pipe(int pipe);
int addclient(int pipe, struct pollfd *fds, struct clientinfo * clients, int nfds);
int cmpfunc(const void *p1, const void *p2);
struct pollfd *shrinkpollarray(struct pollfd *fdarray, int newsize, int nfds);
struct clientinfo *shrinkclientarray(struct clientinfo *clients, int newsize, int nfds);
int parent(int pipefd, int lsnSock, struct sockaddr_in *clientAddr, size_t clientLen, char * keyaschar, unsigned int privatekey);
int serverbroadcast(struct pollfd *pollfds, struct clientinfo *clients, int nfds, char *buffer, int BUFFERSIZE);

#endif