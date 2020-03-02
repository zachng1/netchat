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

struct clientinfo {
    int fd;
    unsigned int secretkey;
    char name[128];
};

//function declarations
int send_fd_pipe(int pipe, int sendfd, struct msghdr * msgh);
struct msghdr * recv_fd_pipe(int pipe);
int addclient(int pipe, struct pollfd *fds, struct clientinfo * clients, int nfds);
int cmpfunc(const void *p1, const void *p2);
struct pollfd *shrinkpollarray(struct pollfd *fdarray, int newsize, int nfds);
struct clientinfo *shrinkclientarray(struct clientinfo *clients, int newsize, int nfds);
int parent(int pipefd, int lsnSock, struct sockaddr_in *clientAddr, size_t clientLen, char * keyaschar, unsigned int privatekey);
int serverbroadcast(struct pollfd *pollfds, struct clientinfo *clients, int nfds, char *buffer, int BUFFERSIZE);

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: server port");
        return 1;
    }
    errno = 0;
    int port = (int)strtol(argv[1], NULL, 0);
    if (errno != 0)
    {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }
    if (port < 0 || port > 65536)
    {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    char buffer[BUFFERSIZE];
    int lsnSock;
    int ioSock;
    int pid;
    int pipefd[2];
    struct sockaddr_in lsnAddr;
    struct sockaddr_in clientAddr;
    size_t clientLen = sizeof(clientAddr);

    //calculate D-H keys
    srand(time(NULL));
    //mod P because it kept overflowing unsigned int
    //not the most secure encryption anyway, this is more of an demonstrative exercise
    unsigned int privatekey = (unsigned int) rand() % P;
    unsigned int publickey = calcPublicKey(privatekey);
    char keyaschar[128];
    sprintf(keyaschar, "%d", publickey);

    

    //setting up server address and client addresses.
    memset(&lsnAddr, 0, sizeof(lsnAddr));
    lsnAddr.sin_family = AF_INET;
    lsnAddr.sin_port = htons(port);
    lsnAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&clientAddr, 0, sizeof(clientAddr));

    if ((lsnSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "socket creation error\n");
        return 1;
    }
    if (bind(lsnSock, (struct sockaddr *)&lsnAddr, sizeof(lsnAddr)) < 0)
    {
        fprintf(stderr, "couldn't bind %d\n", errno);
        return 1;
    }

    if (listen(lsnSock, 5) < 0)
    {
        fprintf(stderr, "couldn't listen\n");
        return -1;
    }
    printf("Listening, ctrl+c to close\n");

    // create separate processes to handle sending and receiving messages
    // while parent can simply listen for new connections to add to chat room.
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd) < 0)
    {
        printf("couldn't create socket");
        return -1;
    }
    pid = fork();

    //parent
    if (pid != 0)
    {
        //close child's side of unix socket
        close(pipefd[1]);
        while (true)
        {
            parent(pipefd[0], lsnSock, &clientAddr, clientLen, keyaschar, privatekey);
        }
    }

    //child -- handles receiving and broadcasting messages.
    else
    {
        //close parent's side of unix socket
        close(pipefd[0]);

        //initialise as 1 because pollfds[0] will be pipe
        //so technically nfds will always be one more than actual nfds
        //-- this is okay because we use it to insert the next one
        int nfds = 1;
        int pollfd_struct_size = 8;
        int recvfd;
        //pollfd will read pipe and client fds
        struct pollfd * pollfds = malloc(sizeof(struct pollfd) * pollfd_struct_size);
        struct clientinfo * clients = malloc(sizeof(struct clientinfo) * pollfd_struct_size);
        

        pollfds[0].fd = pipefd[1];
        pollfds[0].events = POLLIN;
        clients[0].fd = pipefd[1];

        while (true)
        {
            errno = 0;
            if (poll(pollfds, nfds, -1) < 0)
            {
                fprintf(stderr, "poll failed %d\n", errno);
                return -1;
            }
            //if pipe is readable, this means new client - add new client to list of sockets to poll
            if (pollfds[0].revents & POLLIN)
            {
                //case that our filedes struct array is not full -- simple, just add new one to end and increment
                //total number of filedes
                if (nfds < pollfd_struct_size)
                {
                    if (addclient(pipefd[1], pollfds, clients, nfds) < 0) {
                        fprintf(stderr, "error receiving this client\n");
                        return -1;
                    }
                    nfds++;
                }
                //if our filedes struct array is full, there are three options
                //1 there have been disconnects, in which case we can overwrite those with the new one
                //2 there have been LOTS of disconnects, in which case we should shrink the struct
                //3 there have been no disconnects, in which case we should increase the size of the array.
                else
                {
                    //uses -- nfds and returns newnfds
                    //needs pipefd, ** pollfds (so we can change the first pointer?), nfds
                    //first check our new total number of clients
                    int newnfds = 0;
                    bool disconnects = false;
                    for (int i = 0; i < nfds; i++)
                    {
                        if (pollfds[i].fd == -1)
                        {
                            disconnects = true;
                        }
                        else
                            newnfds++;
                    }
                    //this whole thing needs a lot of cleaning but that's a job for another day lol
                    //it just handles all the different resize conditions and resizes pollfd and clientinfo arrays
                    if (disconnects && (newnfds < nfds / 4))
                    {
                        pollfd_struct_size /= 2;
                        struct pollfd *pnew = shrinkpollarray(pollfds, pollfd_struct_size, nfds);
                        struct pollfd *ptemp = pollfds;
                        pollfds = pnew;
                        free(ptemp);

                        struct clientinfo *cnew = shrinkclientarray(clients, pollfd_struct_size, nfds);
                        struct clientinfo *ctemp = clients;
                        clients = cnew;
                        free(ctemp);

                        nfds = newnfds;
                        addclient(pipefd[1], pollfds, clients, nfds);
                        nfds++;
                    }
                    else if (disconnects)
                    {
                        //sort array, shuffling closed sockets to top. they can then be overwritten with incoming fd
                        qsort(pollfds, nfds, sizeof(struct pollfd), cmpfunc);
                        qsort(clients, nfds, sizeof(struct clientinfo), cmpfunc);

                        nfds = newnfds;
                        addclient(pipefd[1], pollfds, clients, nfds);
                        nfds++;
                    }
                    else
                    {
                        //case where no disconnects -- double size of pollfd struct to accomodate, then copy
                        pollfd_struct_size *= 2;
                        struct pollfd *pnew = malloc(sizeof(struct pollfd) * pollfd_struct_size);
                        struct pollfd *ptemp;
                        memcpy(pnew, pollfds, sizeof(struct pollfd) * nfds);
                        ptemp = pollfds;
                        pollfds = pnew;
                        free(ptemp);

                        struct clientinfo *cnew = malloc(sizeof(struct clientinfo) * pollfd_struct_size);
                        struct clientinfo *ctemp;
                        memcpy(cnew, clients, sizeof(struct clientinfo) * nfds);
                        ctemp = clients;
                        clients = cnew;
                        free(ctemp);

                        nfds = newnfds;
                        addclient(pipefd[1], pollfds, clients, nfds);
                        nfds++;
                    }
                }
                //change above to only edit clients struct, then regen pollfd here? 
                //however seems like early optimisation, this works well enough
                for (int i = 0; i < nfds; i++) {
                    printf("N: %s, K: %u, FD: %d\n", clients[i].name, clients[i].secretkey, clients[i].fd);
                }
            }
            //basic loop -- handle receiving and sending messages
            //add logic to handle client info struct array and encode/decode messages + append name to messages
            serverbroadcast(pollfds, clients, nfds, buffer, BUFFERSIZE);
        }
    }
}

//function definitions

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
    strcpy(keybuf, "EMPTY");

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
    printf("Number of clients now %d\n", nfds);
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
    char * keyaschar, 
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
    if (sendxbytes(ioSock, keyaschar, 128) < 0) {
        fprintf(stderr, "Couldn't send own key to client\n");
        return -1;
    }
    if (receivexbytes(ioSock, namebuf, 128) < 0) {
        fprintf(stderr, "Couldn't receive name\n");
        return -1;
    }
    unsigned int clientkey = (unsigned int) strtoul(keybuf, NULL, 0);
    unsigned int secretkey = calcSharedSecret(clientkey, privatekey);
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
    for (int i = 1; i < nfds; i++)
    {
        if (pollfds[i].fd == -1)
            continue;
        if (pollfds[i].revents & POLLIN)
        {
            //if signal recieved, read bytes from that fd into buffer, then subsequently broadcast to all other clients
            errno = 0;
            printf("Message from %s\n", clients[i].name);
            strcpy(buffer, clients[i].name);
            if (receivexbytes(pollfds[i].fd, buffer + strlen(clients[i].name), BUFFERSIZE - strlen(clients[i].name)) < 0)
            {
                //if this function returns less than 0, there was an error or client disconnect.
                pollfds[i].events = 0;
                close(pollfds[i].fd);
                printf("Closed fd %d on read, with errno: %d\n", pollfds[i].fd, errno);
                pollfds[i].fd = -1;
                clients[i].fd = -1;
            }

            //decode func here
            for (int j = 1; j < nfds; j++)
            {
                if (pollfds[j].fd == -1)
                    continue;
                if (j != i)
                {
                    if (sendxbytes(pollfds[j].fd, buffer, BUFFERSIZE) < 0)
                    {
                        //if this function returns less than 0, there was an error or client disconnect.
                        pollfds[j].events = 0;
                        close(pollfds[j].fd);
                        printf("Closed fd %d on send, with errno: %d\n", pollfds[j].fd, errno);
                        pollfds[j].fd = -1;
                        clients[i].fd = -1;
                    }
                }
            }
        }
        //reset revents before next call
        pollfds[i].revents = 0;
        memset(buffer, 0, BUFFERSIZE);
    }
    return 0;
}