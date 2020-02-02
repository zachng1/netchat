#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include "commonfunc.h"

int main(void) {
    char buffer[BUFFERSIZE];
    int lsnSock;
    int ioSock;
    struct sockaddr_in lsnAddr;
    struct sockaddr_in clientAddr;
    size_t clientLen = sizeof(clientAddr);

    //setting up server address and client addresses.
    memset(&lsnAddr, 0, sizeof(lsnAddr));
    lsnAddr.sin_family = AF_INET;
    lsnAddr.sin_port = htons(8080); //should change hardcoded port
    lsnAddr.sin_addr.s_addr = INADDR_ANY;
    memset(&clientAddr, 0, sizeof(clientAddr));

    if ((lsnSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket creation error");
        return 1;
    }
    if (bind(lsnSock, (struct sockaddr *) &lsnAddr, sizeof(lsnAddr)) < 0) {
        fprintf(stderr, "couldn't bind");
        return 1;
    }
    if (listen(lsnSock, 5) < 0) {
        fprintf(stderr, "couldn't listen");
        return 1;
    }
    printf("Listening, ctrl+c to close\n");
    if ((ioSock = accept(lsnSock, (struct sockaddr *) &clientAddr, (socklen_t *) &clientLen)) < 0) {
            fprintf(stderr, "couldn't accept\n");
            return 1;
        }
    printf("connected\n");
    
    //create pollfd for stdin and ioSock
    struct pollfd pollfds[2];
    pollfds[0].fd = 0;
    pollfds[0].events = POLLIN;
    pollfds[1].fd = ioSock;
    pollfds[1].events = POLLIN;


    while(true) {
        if (send_and_receive(pollfds, ioSock, buffer, BUFFERSIZE) < 0) {
            break; //actually should go back to just listening 
        }      
    }
    close(ioSock);
    close(lsnSock);
    return 0;
}