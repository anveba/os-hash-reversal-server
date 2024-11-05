#include "hreversal.h"

#include <assert.h>
#include <endian.h>
#include <memory.h>
#include <openssl/sha.h>

void reverse_hash_simd(uint8_t target_hash[SHA256_LEN],
                       uint64_t start,
                       uint64_t end,
                       uint8_t* abort,
                       struct reversal_result* result)
{
    for (uint64_t i = start; i < end; i++) {

        if (*abort)
            break;

        uint8_t candidate_hash[SHA256_LEN];
        uint64_t le = htole64(i);

        sha256_simd(le, candidate_hash);

        if (!memcmp(candidate_hash, target_hash, SHA256_LEN)) {
            result->success = 1;
            result->result = i;
            return;
        }
    }
    result->success = 0;
}

void reverse_hash_openssl(uint8_t target_hash[SHA256_LEN],
                          uint64_t start,
                          uint64_t end,
                          uint8_t* abort,
                          struct reversal_result* result)
{
    for (uint64_t i = start; i < end; i++) {

        if (*abort)
            break;

        uint8_t candidate_hash[SHA256_LEN];
        uint64_t le = htole64(i);

        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        SHA256_Update(&ctx, &le, 8);
        SHA256_Final(candidate_hash, &ctx);

        if (!memcmp(candidate_hash, target_hash, SHA256_LEN)) {
            result->success = 1;
            result->result = i;
            return;
        }
    }
    result->success = 0;
}