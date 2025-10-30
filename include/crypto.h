#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>

// Encode data - returns newly allocated buffer that must be freed by caller
unsigned char *encode_data(const unsigned char *data, size_t len, size_t *out_len);

// Decode data - returns newly allocated buffer that must be freed by caller
unsigned char *decode_data(const unsigned char *data, size_t len, size_t *out_len);

#endif // CRYPTO_H
