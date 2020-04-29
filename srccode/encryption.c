#include "./encryption.h"
#include <math.h>
#include <stdio.h>

const unsigned int P = 23;
const unsigned int G = 5;

unsigned int calcPublicKey(unsigned int privateKey) {
    unsigned int result = 1;
    //instead of pow to avoid overflow
    for (int i = 0; i < privateKey; i++) {
        result *= G;
        result %= P;
    }
    return result;
}
unsigned int calcSharedSecret(unsigned int partnerKey, unsigned int ownKey) {
    unsigned int result = 1;
    //instead of pow to avoid overflow
    for (int i = 0; i < ownKey; i++) {
        result *= partnerKey;
        result %= P;
    }
    return result;
}
void encryptmessage(char * buffer, size_t bufferlength, unsigned int key) {
    for (int i = 0; i < bufferlength; i++) {
        buffer[i] += key;
    }
}
void decryptmessage(char * buffer, size_t bufferlength, unsigned int key) {
    for (int i = 0; i < bufferlength; i++) {
        buffer[i] -= key;
    }
}