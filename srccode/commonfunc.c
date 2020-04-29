
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <poll.h>
#include <string.h>
#include "commonfunc.h"
#include "encryption.h"

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
        total += sent;
        if (sent == 0) break;
    }
    return total;
}

int send_and_receive(struct pollfd * fds, int socketid, char * const buffer, size_t bufferlen, size_t namelen, unsigned int key) {
    int events = poll(fds, 2, -1);
    // receive before sending -- maybe better? Or order arbitrary?
    if (fds[1].revents & POLLIN) {
        if (receivexbytes(socketid, buffer, bufferlen) < 0) {
            memset(buffer, 0, bufferlen);
            return -1;
        }
        decryptmessage(buffer, bufferlen, key);
        printf("%s", buffer);
        memset(buffer, 0, bufferlen); //clean up buffer for next send / receipt
    }

    if (fds[0].revents & POLLIN) {
        size_t buflencopy = bufferlen;
        getline(buffer, &buflencopy, stdin);
        //getline will adjust the length of buffer accordingly, if size is too long
        //we don't want to deal with messages >1024 bytes.
        if (buflencopy != bufferlen) {
            memset(buffer, 0, bufferlen);
            return -1;
        }
        encryptmessage(buffer, bufferlen - namelen, key);
        if (sendxbytes(socketid, buffer, bufferlen - namelen) < 0) {
            memset(buffer, 0, bufferlen);
            return -1;
        }
        memset(buffer, 0, bufferlen); //clean up buffer for next send / receipt
    }
    return 0;
}