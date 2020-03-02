#include "./encryption.h"
#include <math.h>
#include <stdio.h>

const unsigned int P = 23;
const unsigned int G = 5;

unsigned int calcPublicKey(unsigned int privateKey) {
    return (unsigned int) pow(G, privateKey) % 23;
}
unsigned int calcSharedSecret(unsigned int publicKey, unsigned int privateKey) {
    return (unsigned int) pow(publicKey, privateKey) % P;
}