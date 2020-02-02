#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include "commonfunc.h"

int main(int argc, char * argv[]) {

    //must check that argvs are valid

    int sock;
    struct sockaddr_in serverAddr;
    int len;
    char buffer[BUFFERSIZE];
    //set up server address
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8081); //should change hardcoded port
    
    if ((inet_pton(AF_INET, argv[1], &serverAddr.sin_addr) <= 0)) {
        return 1;
    } 

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return 1;
    }

    if (connect(sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
            return 1;
    }
    printf("Connected!\n");

    //create pollfd for stdin and ioSock
    struct pollfd pollfds[2];
    pollfds[0].fd = 0;
    pollfds[0].events = POLLIN;
    pollfds[1].fd = sock;
    pollfds[1].events = POLLIN;

    while (true) {
        if (send_and_receive(pollfds, sock, buffer, BUFFERSIZE) < 0) {
            break;
        }
    }
    close(sock);
    return 0;

}