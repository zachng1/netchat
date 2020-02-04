#include <stdio.h>
#include <stdlib.h>
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
#include "commonfunc.h"
#include <stropts.h>

int send_fd_pipe(int pipe, int sendfd);
int recv_fd_pipe(int pipe);

int MAXCLIENTS = 10;

int main(int argc, char * argv[]) {
    if (argc != 2) {
        printf("Usage: server port");
        return 1;
    }
    
    int port;
    char buffer[BUFFERSIZE];
    int lsnSock;
    int ioSock;
    int pid;
    int pipefd[2];
    struct sockaddr_in lsnAddr;
    struct sockaddr_in clientAddr;
    size_t clientLen = sizeof(clientAddr);

    
    errno = 0;
    port = (int) strtol(argv[1], NULL, 0);
    if (errno != 0) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }
    if (port < 0 || port > 65536) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    //setting up server address and client addresses.
    memset(&lsnAddr, 0, sizeof(lsnAddr));
    lsnAddr.sin_family = AF_INET;
    lsnAddr.sin_port = htons(port);
    lsnAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&clientAddr, 0, sizeof(clientAddr));

    if ((lsnSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket creation error\n");
        return 1;
    }
    if (bind(lsnSock, (struct sockaddr *) &lsnAddr, sizeof(lsnAddr)) < 0) {
        fprintf(stderr, "couldn't bind %d\n", errno);
        return 1;
    }

    if (listen(lsnSock, 5) < 0) {
        fprintf(stderr, "couldn't listen\n");
        return 1;
    }
    printf("Listening, ctrl+c to close\n");

    
    // create separate processes to handle sending and receiving messages
    // while parent can simply listen for new connections to add to chat room.
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd) < 0) {
        printf("couldn't create socket");
        return -1;
    }
    pid = fork(); 
    
    //parent
    if (pid != 0) {
        //close child's side of unix socket
        close(pipefd[1]);
        int nclients = 0;
        while(true) {
            if ((ioSock = accept(lsnSock, (struct sockaddr *) &clientAddr, (socklen_t *) &clientLen)) < 0) {
                fprintf(stderr, "couldn't accept incoming client\n");
                continue;
            }
            if (nclients < MAXCLIENTS) {
                if (send_fd_pipe(pipefd[0], ioSock) < 0) {
                    return -1;
                }
                printf("added %s to room\n", inet_ntoa(clientAddr.sin_addr));
                close(ioSock);
                nclients++;
            }
            else {
                printf("Room is full\n");
            }
        }
    }

    //child -- handles receiving and broadcasting messages.
    else {
        //close parent's side of unix socket
        close(pipefd[0]);

        //initialise as 1 because pollfds[0] will be pipe
        int nfds = 1;
        int recvfd;
        //pollfd will read pipe and client fds
        struct pollfd pollfds[MAXCLIENTS + 1];

        pollfds[0].fd = pipefd[1];
        pollfds[0].events = POLLIN;
        while (true) {
            if (poll(pollfds, nfds, -1) < 0) {
                return -1;
            }
            //if pipe is readable, this means new client - add new client to list of sockets to poll
            if (pollfds[0].revents & POLLIN) {
                // double check we don't have MAXCLIENTS, even though parent should confirm before sending
                if (nfds != MAXCLIENTS) {
                    if ((recvfd = recv_fd_pipe(pipefd[1])) < 0) {
                        fprintf(stderr, "couldn't receive %d\n", recvfd);
                        return -1;
                    }
                    pollfds[nfds].fd = recvfd;
                    pollfds[nfds].events = POLLIN;
                    pollfds[nfds].revents = 0;
                    nfds++;
                }
            }

            for (int i = 1; i < nfds; i++) {
                if (pollfds[i].revents & POLLIN) {
                    if (receivexbytes(pollfds[i].fd, buffer, BUFFERSIZE) < 0) {
                        //if this function returns less than 0, there was an error or client disconnect. In lieu of reshuffling array down on each disconnect, temp measure here is to no longer care about it.
                        pollfds[i].events = 0;
                    }
                    for (int j = 1; j < nfds; j++) {
                        if (j != i) {
                            if (sendxbytes(pollfds[j].fd, buffer, BUFFERSIZE) < 0) {
                                //if this function returns less than 0, there was an error or client disconnect. In lieu of reshuffling array down on each disconnect, temp measure here is to no longer care about it.
                                pollfds[j].events = 0;
                            }
                        }
                    }
                    //reset revents before next call
                    pollfds[i].revents = 0;
                    memset(buffer, 0, BUFFERSIZE);
                }
            }
            //add code to deal with clients dropping
            //add code to identify clients by name --- could send via pipe as a header?
        }
    }
}

// adapted from http://man7.org/tlpi/code/online/dist/sockets/scm_rights_send.c.html
int send_fd_pipe(int pipe, int sendfd) {
    struct msghdr msgh;
    struct iovec iov;
    int iovdata;
    int sent;

    //need to define this as a union so that the message is aligned properly -- i dont quite get why, but oh well
    union {
        char container[CMSG_SPACE(sizeof(int))];
        struct cmsghdr fill;
    } cMsg;
    struct cmsghdr * cmsgptr;

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;

    //need to send some 'real' data in the message in order to transmit the control message header which includes the fd
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    iov.iov_base = &iovdata;
    iov.iov_len = sizeof(int);
    iovdata = 69;

    msgh.msg_control = cMsg.container;
    msgh.msg_controllen = sizeof(cMsg.container);

    cmsgptr = CMSG_FIRSTHDR(&msgh);
    cmsgptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmsgptr->cmsg_level = SOL_SOCKET;
    cmsgptr->cmsg_type = SCM_RIGHTS;

    // cast the pointer that cmsg_data points to to an int, then dereference it
    * ((int *) CMSG_DATA(cmsgptr)) = sendfd;
    
    // having constructed the msgheader and the control msghdr ancillary data, we send it down the pipe to child process.
    if ((sent = sendmsg(pipe, &msgh, 0)) < 0) {
        return -1;
    }
    return 0;
}

//adapted from http://man7.org/tlpi/code/online/dist/sockets/scm_rights_recv.c.html
int recv_fd_pipe(int pipe) {
    int recvfd;
    int nr;
    struct msghdr msgh;
    struct iovec iov;
    int iovdata;
    union {
        char container[CMSG_SPACE(sizeof(int))];
        struct cmsghdr fill;
    } cMsg;
    struct cmsghdr * cmsgptr;

    //set up message to recieve data from parent
    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    iov.iov_base = &iovdata;
    iov.iov_len = sizeof(int);

    msgh.msg_control = cMsg.container;
    msgh.msg_controllen = sizeof(cMsg.container);

    //receive data and fd
    if ((nr = recvmsg(pipe, &msgh, 0)) < 0) {
        return -1;
    }

    cmsgptr = CMSG_FIRSTHDR(&msgh);
    if (cmsgptr == NULL || cmsgptr->cmsg_len != CMSG_LEN(sizeof(int))) {
        return -1;
    }
    if (cmsgptr->cmsg_level != SOL_SOCKET) {
        return -2;
    }
    if (cmsgptr->cmsg_type != SCM_RIGHTS) {
        return -3;
    }

    if ((recvfd = (* ((int *) CMSG_DATA(cmsgptr)))) < 0) {
        return -4;
    }
    else {
        return recvfd;
    }
}