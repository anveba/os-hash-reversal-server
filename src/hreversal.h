#ifndef HREVERSAL_H_INCLUDED
#define HREVERSAL_H_INCLUDED

#include "sha256.h"

struct reversal_result
{
    uint8_t success;
    uint64_t result;
};

void reverse_hash(uint8_t target_hash[SHA256_LEN],
                  uint64_t start,
                  uint64_t end,
                  uint8_t* abort,
                  struct reversal_result* result);

#endif