#ifndef SHA256_H_INCLUDED
#define SHA256_H_INCLUDED

#include <stdint.h>

#define SHA256_LEN 32

void sha256_simd(uint64_t input, uint8_t hash[SHA256_LEN]);

void hash_to_str(char* str, uint8_t hash[SHA256_LEN]);

void hash_prepare_for_simd(uint8_t hash[SHA256_LEN]);

#endif