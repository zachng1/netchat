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
#include <errno.h>
#include <time.h>
#include <limits.h>
#include "commonfunc.h"
#include "encryption.h"

int main(int argc, char * argv[]) {

    if (argc != 4) {
        fprintf(stderr, "Usage: client ipaddress port displayname");
        return -1;
    }
    else if (strlen(argv[3]) > 127) {
        fprintf(stderr, "Name too long\n");
        return -1;
    }

    int port;
    int sock;
    struct sockaddr_in serverAddr;
    int len;
    char buffer[BUFFERSIZE];
    char name[128];
    strcpy(name, argv[3]);
    char keybuf[128];
    char serverkeybuf[128];

    //calculate D-H keys
    srand(time(NULL));
    //mod P because it kept overflowing unsigned int
    //not the most secure encryption anyway, this is more of an demonstrative exercise
    unsigned int privatekey = (unsigned int) rand() % P;
    unsigned int publickey = calcPublicKey(privatekey);
    sprintf(keybuf, "%d", publickey); 
    


    errno = 0;
    port = (int) strtol(argv[2], NULL, 0);
    if (errno != 0) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }
    if (port < 0 || port > 65536) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    //set up server address
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port); //should change hardcoded port
    
    if ((inet_pton(AF_INET, argv[1], &serverAddr.sin_addr) <= 0)) {
        fprintf(stderr, "invalid ip address\n");
        return 1;
    } 

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return 1;
    }

    if (connect(sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
        return 1;
    }
    printf("Connected! Send EXIT to close.\n");

    sendxbytes(sock, keybuf, 128);
    receivexbytes(sock, serverkeybuf, 128);
    sendxbytes(sock, name, 128);
    unsigned int serverkey = (unsigned int) strtoul(serverkeybuf, NULL, 0);
    unsigned int secretkey = calcSharedSecret(serverkey, privatekey);

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
    shutdown(sock, SHUT_RDWR);
    close(sock);
    return 0;

}