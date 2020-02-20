#ifndef COMMON_FUNC
#define COMMON_FUNC

#include <stdbool.h>
#include <stdlib.h>

extern const size_t BUFFERSIZE;
int receivexbytes(int socketid, char * buff, size_t bufflen);
int sendxbytes(int socketid, char * buff, size_t bufflen);
int send_and_receive(struct pollfd * fds, int socketid, char * buffer, size_t bufferlen, const char * name);
#endif