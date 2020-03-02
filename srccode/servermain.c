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
#include "server.h"

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
            }
            //basic loop -- handle receiving and sending messages
            //add logic to handle client info struct array and encode/decode messages + append name to messages
            serverbroadcast(pollfds, clients, nfds, buffer, BUFFERSIZE);
        }
    }
}