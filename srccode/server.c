#include "server.h"

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
#include <stropts.h>
#include <time.h>
#include "commonfunc.h"
#include "encryption.h"

// adapted from http://man7.org/tlpi/code/online/dist/sockets/scm_rights_send.c.html
// expects msghdr already has msg_iov & msg_iolen fields filled
int send_fd_pipe(int pipe, int sendfd, struct msghdr * msgh)
{
    int sent;

    //need to define this as a union so that the message is aligned properly -- i dont quite get why, but source says its needed
    union {
        char container[CMSG_SPACE(sizeof(int))];
        struct cmsghdr fill;
    } cMsg;
    struct cmsghdr *cmsgptr;

    msgh->msg_name = NULL;
    msgh->msg_namelen = 0;

    msgh->msg_control = cMsg.container;
    msgh->msg_controllen = sizeof(cMsg.container);

    cmsgptr = CMSG_FIRSTHDR(msgh);
    cmsgptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmsgptr->cmsg_level = SOL_SOCKET;
    cmsgptr->cmsg_type = SCM_RIGHTS;

    // cast the pointer that cmsg_data points to to an int, then dereference it
    *((int *)CMSG_DATA(cmsgptr)) = sendfd;

    // having constructed the msgheader and the control msghdr ancillary data, we send it down the pipe to child process.
    while (true) {
        sent = sendmsg(pipe, msgh, 0);
        if (sent < 0) return -1;
        else return 0;
    }
}

//adapted from http://man7.org/tlpi/code/online/dist/sockets/scm_rights_recv.c.html
struct msghdr * recv_fd_pipe(int pipe)
{
    int recvfd;
    int nr;
    struct msghdr * msgh = malloc(sizeof(struct msghdr));
    
    union {
        char container[CMSG_SPACE(sizeof(int))];
        struct cmsghdr fill;
    } cMsg;

    struct iovec * iov = malloc(sizeof(struct iovec) * 2);
    struct msghdr * metadata;
    //very basic implementation of dh exchange atm
    char keybuf[128];
    char namebuf[128];

    //set up message to recieve data from parent
    msgh->msg_name = NULL;
    msgh->msg_namelen = 0;
    msgh->msg_iov = iov;
    msgh->msg_iovlen = 2;
    iov[0].iov_base = namebuf;
    iov[0].iov_len = 128;
    iov[1].iov_base = keybuf;
    iov[1].iov_len = 128;

    msgh->msg_control = cMsg.container;
    msgh->msg_controllen = sizeof(cMsg.container);

    //receive data and fd
    nr = recvmsg(pipe, msgh, 0);
    if (nr < 0) return NULL;
    return msgh;
}

int addclient(int pipe, struct pollfd *fds, struct clientinfo * clients, int nfds)
{
    struct msghdr * msgh;
    struct cmsghdr * cmsgptr;
    int recvfd;

    msgh = recv_fd_pipe(pipe);
    if (msgh == NULL)
    {
        fprintf(stderr, "couldn't receive %d\n", recvfd);
        return -1;
    }
    cmsgptr = CMSG_FIRSTHDR(msgh);
    if (cmsgptr == NULL || cmsgptr->cmsg_len != CMSG_LEN(sizeof(int)))
    {
        return -1;
    }
    if (cmsgptr->cmsg_level != SOL_SOCKET)
    {
        return -1;
    }
    if (cmsgptr->cmsg_type != SCM_RIGHTS)
    {
        return -1;
    }
    if ((recvfd = (*((int *)CMSG_DATA(cmsgptr)))) < 0)
    {
        return -1;
    }

    fds[nfds].fd = recvfd;
    fds[nfds].events = POLLIN;
    fds[nfds].revents = 0;

    clients[nfds].fd = recvfd;
    clients[nfds].secretkey = (unsigned int) strtoul(msgh->msg_iov[1].iov_base, NULL, 0);
    strcpy(clients[nfds].name, msgh->msg_iov[0].iov_base);
    printf("Number of clients now %d.\n", nfds);
    free(msgh);
    return 0;
}

int cmpfunc(const void *p1, const void *p2)
{
    if (((struct pollfd *)p1)->fd == -1)
    {
        return 1;
    }
    else
        return 0;
}

struct pollfd *shrinkpollarray(struct pollfd *fdarray,
                           int newsize,
                           int nfds)
{
    if (nfds > newsize)
        return NULL; //should not be shrinking if more fds than shrink size

    struct pollfd *new = malloc(sizeof(struct pollfd) * (newsize));
    for (int i = 0, j = 0; i < nfds; i++)
    {
        //copy only non disconnected
        if (fdarray[i].fd > -1)
        {
            new[j++].fd == fdarray[i].fd;
        }
    }
    return new;
}

struct clientinfo *shrinkclientarray(struct clientinfo *clients, int newsize, int nfds) {
    if (nfds > newsize) return NULL; //same as shrink poll -- shouldn't shrink if nfds larger than size

    struct clientinfo *new = malloc(sizeof(struct clientinfo) * newsize);
    int j = 0;
    for (int i = 0, j = 0; i < nfds; i++)
    {
        //copy only non disconnected
        if (clients[i].fd > -1)
        {
            new[j].fd = clients[i].fd;
            new[j].secretkey = clients[i].secretkey;
            strcpy(new[j].name, clients[i].name);
            j++;
        }
    }
    return new;

}

int parent(
    int pipefd, 
    int lsnSock, 
    struct sockaddr_in *clientAddr, 
    size_t clientLen, 
    char * publickeyaschar, 
    unsigned int privatekey)
{
    int ioSock;
    struct iovec * iov = malloc(sizeof(struct iovec) * 2);
    struct msghdr * metadata = malloc(sizeof(struct msghdr));
    char namebuf[128];
    //very basic implementation of dh exchange atm
    char keybuf[128];

    errno = 0;
    if ((ioSock = accept(lsnSock, (struct sockaddr *)clientAddr, (socklen_t *)&clientLen)) < 0)
    {
        fprintf(stderr, "couldn't accept incoming client %d\n", errno);
        return -1;
    }
    if (receivexbytes(ioSock, keybuf, 128) < 0) {
        fprintf(stderr, "Couldn't receive client's key\n");
        return -1;
    }
    if (sendxbytes(ioSock, publickeyaschar, 128) < 0) {
        fprintf(stderr, "Couldn't send own key to client\n");
        return -1;
    }
    //need to encode
    if (receivexbytes(ioSock, namebuf, 128) < 0) {
        fprintf(stderr, "Couldn't receive name\n");
        return -1;
    }
    unsigned int clientkey = (unsigned int) strtoul(keybuf, NULL, 0);
    unsigned int secretkey = calcSharedSecret(clientkey, privatekey);
    //in case of last client key still being in keybuffer.
    memset(keybuf, 0, 128);
    sprintf(keybuf, "%d", secretkey);



    metadata->msg_iov = iov;
    metadata->msg_iovlen = 2; //public key, client name
    iov[0].iov_base = namebuf;
    iov[0].iov_len = 128;
    iov[1].iov_base = keybuf;
    iov[1].iov_len = 128;
    

    errno = 0;
    if (send_fd_pipe(pipefd, ioSock, metadata) < 0)
    {
        fprintf(stderr, "Couldn't send to child %d\n", errno);
        return -1;
    }
    close(ioSock);
    free(metadata);
    return 0;
}

int serverbroadcast(struct pollfd *pollfds, struct clientinfo *clients, int nfds, char *buffer, int BUFFERSIZE)
{
    char * bufferpointer;
    int namelen, total;
    for (int i = 1; i < nfds; i++)
    {
        bufferpointer = buffer;
        if (pollfds[i].fd == -1)
            continue;
        if (pollfds[i].revents & POLLIN)
        {
            //if signal recieved, read bytes from that fd into buffer, then subsequently broadcast to all other clients
            errno = 0;
            
            namelen = sprintf(buffer, "%s: ", clients[i].name);
            bufferpointer += namelen;
            if ((total = receivexbytes(pollfds[i].fd, bufferpointer, BUFFERSIZE - namelen)) < 0)
            {
                //if this function returns less than 0, there was an error or client disconnect.
                pollfds[i].events = 0;
                close(pollfds[i].fd);
                printf("Closed fd %d on read, with errno: %d\n", pollfds[i].fd, errno);
                pollfds[i].fd = -1;
                clients[i].fd = -1;
            }
            decryptmessage(bufferpointer, BUFFERSIZE - namelen, clients[i].secretkey);

            //decode func here
            for (int j = 1; j < nfds; j++)
            {
                //check we didn't receive a close signal on read -- if so, skip broadcast
                if (pollfds[i].fd == -1) 
                    break;
                if (pollfds[j].fd == -1)
                    continue;
                if (j != i)
                {
                    encryptmessage(buffer, BUFFERSIZE, clients[j].secretkey);
                    if (sendxbytes(pollfds[j].fd, buffer, BUFFERSIZE) < 0)
                    {
                        //if this function returns less than 0, there was an error or client disconnect.
                        pollfds[j].events = 0;
                        close(pollfds[j].fd);
                        printf("Closed fd %d on send, with errno: %d\n", pollfds[j].fd, errno);
                        pollfds[j].fd = -1;
                        clients[j].fd = -1;
                    }
                    //need to decrypt so that unique encryption for other clients will work
                    decryptmessage(buffer, BUFFERSIZE, clients[j].secretkey);
                }
            }
        }
         //reset revents before next call
        pollfds[i].revents = 0;
        memset(buffer, 0, BUFFERSIZE);
    }
    return 0;
}