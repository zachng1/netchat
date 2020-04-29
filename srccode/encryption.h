#ifndef ENCRYPTION
#define ENCRYPTION

#include <stdlib.h>
#include <math.h>

extern const unsigned int P;
extern const unsigned int G;

unsigned int calcPublicKey(unsigned int privateKey);
unsigned int calcSharedSecret(unsigned int partnerKey, unsigned int ownKey);
void encryptmessage(char * buffer, size_t bufferlength, unsigned int key);
void decryptmessage(char * buffer, size_t bufferlength, unsigned int key);


#endif