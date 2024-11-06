#ifndef SHA256_H_INCLUDED
#define SHA256_H_INCLUDED

#include <stdint.h>

#define SHA256_LEN 32
#define OUTPUT_VECS 8

#define VEC_SIZE 8
typedef uint32_t vec_t __attribute__((vector_size(VEC_SIZE * sizeof(uint32_t))));

void sha256_simd(uint64_t input[VEC_SIZE], vec_t hash[OUTPUT_VECS]);

void hash_to_str(char* str, uint8_t hash[SHA256_LEN]);

void hash_prepare_for_simd(uint8_t hash[SHA256_LEN]);

#endif