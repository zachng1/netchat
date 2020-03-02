#ifndef ENCRYPTION
#define ENCRYPTION

#include <stdlib.h>
#include <math.h>

extern const unsigned int P;
extern const unsigned int G;

unsigned int calcPublicKey(unsigned int privateKey);
unsigned int calcSharedSecret(unsigned int publicKey, unsigned int privateKey);
char * encryptmessage(const char * message, unsigned int key);
char * decryptmessage(const char * emessage, unsigned int key);


#endif