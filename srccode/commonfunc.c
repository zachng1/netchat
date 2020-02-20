
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <poll.h>
#include <string.h>
#include "commonfunc.h"

const size_t BUFFERSIZE = 1024;

int receivexbytes(int socketid, char * buff, size_t bufflen) {
    size_t total = 0;
    size_t received;
    while (total < bufflen) {
        if ((received = recv(socketid, buff, bufflen, 0)) <= 0) {
            return -1; //error or client disconnect
        }
        total += received;
        buff += received;
    }
    return total;
}

int sendxbytes(int socketid, char * buff, size_t bufflen) {
    size_t total = 0;
    size_t sent;
    while (total < bufflen) {
        if (sent = send(socketid, buff, bufflen, 0) < 0) {
            return -1;
        }
        if (sent == 0) break;
        total += sent;
    }
    return total;
}

int send_and_receive(struct pollfd * fds, int socketid, char * const buffer, size_t bufferlen, const char * name) {
    char * bufferpointer = buffer;
    int namelen;
    int events = poll(fds, 2, -1);
    // receive before sending -- maybe better? Or order arbitrary?
    if (fds[1].revents & POLLIN) {
        if (receivexbytes(socketid, buffer, bufferlen) < 0) {
            return -1;
        }
        printf("%s", buffer);
        memset(buffer, 0, bufferlen); //clean up buffer for next send / receipt
    }

    if (fds[0].revents & POLLIN) {
        //write user's name to front of buffer before sending -- so other users know who said what
        namelen = sprintf(buffer, "%s: ", name);
        bufferpointer += namelen;

        fgets(bufferpointer, ((int) bufferlen) - namelen, stdin); 
        if (strcmp(bufferpointer, "EXIT\n") == 0) {
            printf("EXIT received, shutdown\n");
            return -1;
        }
        else if (sendxbytes(socketid, buffer, bufferlen) < 0) {
            return -1;
        }
        memset(buffer, 0, bufferlen); //clean up buffer for next send / receipt
    }
    return 0;
}