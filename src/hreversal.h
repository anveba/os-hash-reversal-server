#ifndef HREVERSAL_H_INCLUDED
#define HREVERSAL_H_INCLUDED

#include "messages.h"

uint64_t reverse_hash(uint8_t target_hash[SHA256_DIGEST_LENGTH], uint64_t start, uint64_t end);

#endif