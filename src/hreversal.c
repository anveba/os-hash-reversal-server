#include <assert.h>

#include "hreversal.h"

static int hash_equals(uint8_t first[SHA256_DIGEST_LENGTH], uint8_t second[SHA256_DIGEST_LENGTH])
{
    uint64_t* f64 = (uint64_t*)first;
    uint64_t* s64 = (uint64_t*)second;
    return f64[0] == s64[0] &&
           f64[1] == s64[1] &&
           f64[2] == s64[2] &&
           f64[3] == s64[3];
}

uint64_t reverse_hash(uint8_t target_hash[SHA256_DIGEST_LENGTH], uint64_t start, uint64_t end)
{
    for (uint64_t i = start; i < end; i++) {
        uint64_t le_candidate = htole64(i);
        uint8_t candidate_hash[SHA256_DIGEST_LENGTH];
        SHA256((unsigned char*)&le_candidate, sizeof(uint64_t), candidate_hash);
        if (hash_equals(candidate_hash, target_hash))
            return i;
    }
    assert(0);
    return 0;
}