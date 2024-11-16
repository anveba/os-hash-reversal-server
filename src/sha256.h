#ifndef SHA256_H_INCLUDED
#define SHA256_H_INCLUDED

#include <stdint.h>

#define SHA256_LEN 32
#define OUTPUT_VECS 8
#define MSG_SIZE 16

// Change this as needed
#define VEC_SIZE 8

typedef uint32_t vec_t __attribute__((vector_size(VEC_SIZE * sizeof(uint32_t))));
typedef uint64_t vec64_t __attribute__((vector_size(VEC_SIZE * sizeof(uint64_t))));

void sha256_vectorized(const vec_t initials[MSG_SIZE], vec_t hash[OUTPUT_VECS]);

void sha256_init_msg(vec_t msg[MSG_SIZE]);

void sha256_load_input(vec_t msg[MSG_SIZE], const vec64_t* input);

void sha256_openssl(uint64_t input, uint8_t hash[SHA256_LEN]);

void hash_to_str(char* str, uint8_t hash[SHA256_LEN]);

void hash_preprocess(uint8_t hash[SHA256_LEN]);

#if SB_VECTORIZE
#define SB_SHA256_VECTORIZED
#define SB_HASH_NEEDS_PREPROCESSING
#endif

#endif