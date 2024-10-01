#include "hreversal.h"

#include <assert.h>
#include <endian.h>

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