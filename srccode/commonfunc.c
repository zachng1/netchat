
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <poll.h>
#include <string.h>
#include "commonfunc.h"

const size_t BUFFERSIZE = 1024;

bool receivexbytes(int socketid, char * buff, size_t bufflen) {
    size_t total = 0;
    size_t received;
    while (total < bufflen) {
        if ((received = recv(socketid, buff, bufflen, 0)) <= 0) {
            return -1;
        } 
        if (received == 0) {
            return -1; //end of file reached
        }
        total += received;
        buff += received;
    }
}

bool sendxbytes(int socketid, char * buff, size_t bufflen) {
    size_t total = 0;
    size_t sent;
    while (total < bufflen) {
            if (sent = send(socketid, buff, bufflen, 0) < 0) {
                return 1;
            }
            if (sent == 0) break;
            total += sent;
        }
}

int send_and_receive(struct pollfd * fds, int socketid, char * buffer, size_t bufferlen) {
    int events = poll(fds, 2, -1);
    // receive before sending -- maybe better? Or order arbitrary?
    if (fds[1].revents & POLLIN) {
        if (receivexbytes(socketid, buffer, bufferlen) < 0) {
            return -1;
        }
        printf("Received message: %s", buffer);
        memset(buffer, 0, bufferlen); //clean up buffer for next send / receipt
    }

    if (fds[0].revents & POLLIN) {
        fgets(buffer, bufferlen, stdin); 
        if (sendxbytes(socketid, buffer, bufferlen) < 0) {
            return -1;
        }
        memset(buffer, 0, bufferlen); //clean up buffer for next send / receipt
    }
    return 0;
}