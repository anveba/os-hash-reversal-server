#include "hreversal.h"

#include <assert.h>
#include <endian.h>

static int hash_equals(uint8_t first[SHA256_LEN], uint8_t second[SHA256_LEN])
{
    uint64_t* f64 = (uint64_t*)first;
    uint64_t* s64 = (uint64_t*)second;
    return f64[0] == s64[0] &&
           f64[1] == s64[1] &&
           f64[2] == s64[2] &&
           f64[3] == s64[3];
}

void reverse_hash(uint8_t target_hash[SHA256_LEN],
                  uint64_t start,
                  uint64_t end,
                  uint8_t* abort,
                  struct reversal_result* result)
{
    for (uint64_t i = start; i < end; i++) {
        if (*abort)
            break;
        uint8_t candidate_hash[SHA256_LEN];
        sha256(htole64(i), candidate_hash);
        if (hash_equals(candidate_hash, target_hash)) {
            result->success = 1;
            result->result = i;
            return;
        }
    }
    result->success = 0;
}