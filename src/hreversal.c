#include "hreversal.h"

#include <assert.h>
#include <endian.h>
#include <memory.h>

#ifdef SB_SHA256_VECTORIZED

void reverse_hash(uint8_t target_hash[SHA256_LEN],
                  uint64_t start,
                  uint64_t end,
                  uint8_t* abort,
                  struct reversal_result* result)
{
    // Prepare hash vectors for efficient comparison
    vec_t target_hash_vectors[OUTPUT_VECS];
    for (int i = 0; i < OUTPUT_VECS; i++)
        for (int j = 0; j < VEC_SIZE; j++)
            memcpy(&target_hash_vectors[i][j], target_hash + i * sizeof(uint32_t), sizeof(uint32_t));

    // Calculate number of rounds
    uint64_t rounds = end - start;
    if (rounds % VEC_SIZE == 0)
        rounds = rounds / VEC_SIZE;
    else
        rounds = rounds / VEC_SIZE + 1;

    // Prepare input
    vec_t msg[MSG_SIZE];
    sha256_init_msg(msg);

    vec64_t input;
    for (int i = 0; i < VEC_SIZE; i++)
        input[i] = start + i;

    // Main bruteforce loop
    for (uint64_t i = 0; i < rounds; i++) {

        sha256_load_input(msg, &input);

        vec_t candidate_hash[OUTPUT_VECS];
        sha256_vectorized(msg, candidate_hash);

        if ((i & 7) == 0 && *abort)
            break;

        // Compare against target
        vec_t cmp_res = (candidate_hash[0] == target_hash_vectors[0]);
#pragma GCC unroll 128
        for (int j = 1; j < OUTPUT_VECS; j++)
            cmp_res = (cmp_res & (candidate_hash[j] == target_hash_vectors[j]));

        for (int j = 0; j < VEC_SIZE; j++) {
            if (cmp_res[j]) {
                result->success = 1;
                result->result = start + i * VEC_SIZE + j;
                return;
            }
        }
        input += VEC_SIZE;
    }
    result->success = 0;
}

#else

void reverse_hash(uint8_t target_hash[SHA256_LEN],
                  uint64_t start,
                  uint64_t end,
                  uint8_t* abort,
                  struct reversal_result* result)
{
    for (uint64_t i = start; i < end; i++) {

        if ((i & 7) == 0 && *abort)
            break;

        uint8_t candidate_hash[SHA256_LEN];
        uint64_t le = htole64(i);

#if SB_USE_x86_64_SHA_SIMD
        sha256_x86_64(le, candidate_hash);
#elif SB_USE_NAIVE_SHA
        sha256_naive(le, candidate_hash);
#else
        sha256_openssl(le, candidate_hash);
#endif

        if (!memcmp(candidate_hash, target_hash, SHA256_LEN)) {
            result->success = 1;
            result->result = i;
            return;
        }
    }
    result->success = 0;
}

#endif