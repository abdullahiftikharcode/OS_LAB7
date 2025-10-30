#include "crypto.h"
#include <stdlib.h>
#include <string.h>

// Simple XOR-based encoding/decoding with a fixed key
// In a real application, use a proper encryption library like OpenSSL

// Simple XOR cipher with a fixed key
static void xor_cipher(const unsigned char *input, unsigned char *output, size_t len, const char *key) {
    size_t key_len = strlen(key);
    for (size_t i = 0; i < len; i++) {
        output[i] = input[i] ^ key[i % key_len];
    }
}

// Encode data - returns newly allocated buffer that must be freed by caller
unsigned char *encode_data(const unsigned char *data, size_t len, size_t *out_len) {
    const char *key = "MySecretKey123";  // In a real app, use a secure key management system
    *out_len = len;
    unsigned char *encoded = (unsigned char *)malloc(*out_len);
    if (!encoded) return NULL;
    
    xor_cipher(data, encoded, len, key);
    return encoded;
}

// Decode data - returns newly allocated buffer that must be freed by caller
unsigned char *decode_data(const unsigned char *data, size_t len, size_t *out_len) {
    // Decoding is the same as encoding with XOR
    return encode_data(data, len, out_len);
}
