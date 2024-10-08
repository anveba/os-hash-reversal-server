#ifndef SHA256_H_INCLUDED
#define SHA256_H_INCLUDED

#include <stdint.h>

#define SHA256_LEN 32
#define OUTPUT_VECS 8

#define VEC_SIZE 8
typedef uint32_t vec_t __attribute__((vector_size(VEC_SIZE * sizeof(uint32_t))));

void sha256_simd(uint64_t input[VEC_SIZE], vec_t hash[OUTPUT_VECS]);

void hash_to_str(char* str, uint8_t hash[SHA256_LEN]);

static inline int hash_equals(uint8_t first[SHA256_LEN], uint8_t second[SHA256_LEN])
{
    uint64_t* f64 = (uint64_t*)first;
    uint64_t* s64 = (uint64_t*)second;
    return f64[0] == s64[0] &&
           f64[1] == s64[1] &&
           f64[2] == s64[2] &&
           f64[3] == s64[3];
}

void hash_prepare(uint8_t hash[SHA256_LEN]);

#endif