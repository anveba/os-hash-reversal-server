#include "sha256.h"
#include <stdio.h>

#include <endian.h>
#include <immintrin.h>
#include <memory.h>
#include <openssl/sha.h>

// Reference: https://en.wikipedia.org/wiki/SHA-2
//            https://gcc.gnu.org/onlinedocs/gcc/Vector-Extensions.html

static const uint32_t k[64] = {
    0x428a2f98,
    0x71374491,
    0xb5c0fbcf,
    0xe9b5dba5,
    0x3956c25b,
    0x59f111f1,
    0x923f82a4,
    0xab1c5ed5,
    0xd807aa98,
    0x12835b01,
    0x243185be,
    0x550c7dc3,
    0x72be5d74,
    0x80deb1fe,
    0x9bdc06a7,
    0xc19bf174,
    0xe49b69c1,
    0xefbe4786,
    0x0fc19dc6,
    0x240ca1cc,
    0x2de92c6f,
    0x4a7484aa,
    0x5cb0a9dc,
    0x76f988da,
    0x983e5152,
    0xa831c66d,
    0xb00327c8,
    0xbf597fc7,
    0xc6e00bf3,
    0xd5a79147,
    0x06ca6351,
    0x14292967,
    0x27b70a85,
    0x2e1b2138,
    0x4d2c6dfc,
    0x53380d13,
    0x650a7354,
    0x766a0abb,
    0x81c2c92e,
    0x92722c85,
    0xa2bfe8a1,
    0xa81a664b,
    0xc24b8b70,
    0xc76c51a3,
    0xd192e819,
    0xd6990624,
    0xf40e3585,
    0x106aa070,
    0x19a4c116,
    0x1e376c08,
    0x2748774c,
    0x34b0bcb5,
    0x391c0cb3,
    0x4ed8aa4a,
    0x5b9cca4f,
    0x682e6ff3,
    0x748f82ee,
    0x78a5636f,
    0x84c87814,
    0x8cc70208,
    0x90befffa,
    0xa4506ceb,
    0xbef9a3f7,
    0xc67178f2
};

#define RROT(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

// SHA256 hashing function optimised for a 64-bit input size and uses SIMD.
// Specialised SHA256 SIMD instructions exist on some architectures, but not
// all, so this implementation does not use them. This function does not
// compute the final part of the SHA256 hash, as this is reversable.
void sha256_vectorized(const vec_t initial_msg[MSG_SIZE], vec_t hash[OUTPUT_VECS])
{
    const uint32_t hs[8] = { 0x6a09e667,
                             0xbb67ae85,
                             0x3c6ef372,
                             0xa54ff53a,
                             0x510e527f,
                             0x9b05688c,
                             0x1f83d9ab,
                             0x5be0cd19 };

    // Prepare first 512-bit chunk with proper endianness
    vec_t msg[MSG_SIZE];
    memcpy(msg, initial_msg, sizeof(msg));

    // Prepare the initial state
#pragma GCC unroll 128
    for (int i = 0; i < VEC_SIZE; i++) {
        hash[0][i] = hs[0];
        hash[1][i] = hs[1];
        hash[2][i] = hs[2];
        hash[3][i] = hs[3];
        hash[4][i] = hs[4];
        hash[5][i] = hs[5];
        hash[6][i] = hs[6];
        hash[7][i] = hs[7];
    }

// Main loops
#pragma GCC unroll 16
    for (int i = 0; i < 16; i++) {
        vec_t s1 = RROT(hash[(4 + 7 * i) % 8], 6) ^ RROT(hash[(4 + 7 * i) % 8], 11) ^ RROT(hash[(4 + 7 * i) % 8], 25);
        vec_t ch = (hash[(4 + 7 * i) % 8] & hash[(5 + 7 * i) % 8]) ^ ((~hash[(4 + 7 * i) % 8]) & hash[(6 + 7 * i) % 8]);
        vec_t temp1 = hash[(7 + 7 * i) % 8] + s1 + ch + k[i] + msg[i % MSG_SIZE];
        vec_t s0 = RROT(hash[(0 + 7 * i) % 8], 2) ^ RROT(hash[(0 + 7 * i) % 8], 13) ^ RROT(hash[(0 + 7 * i) % 8], 22);
        vec_t maj = (hash[(0 + 7 * i) % 8] & hash[(1 + 7 * i) % 8]) ^ (hash[(0 + 7 * i) % 8] & hash[(2 + 7 * i) % 8]) ^ (hash[(1 + 7 * i) % 8] & hash[(2 + 7 * i) % 8]);
        vec_t temp2 = s0 + maj;
        hash[(3 + 7 * i) % 8] = hash[(3 + 7 * i) % 8] + temp1;
        hash[(7 + 7 * i) % 8] = temp1 + temp2;
    }

#pragma GCC unroll 48
    for (int i = 16; i < 64; i++) {
        vec_t s0 = RROT(msg[(i + 1) % MSG_SIZE], 7) ^ RROT(msg[(i + 1) % MSG_SIZE], 18) ^ (msg[(i + 1) % MSG_SIZE] >> 3);
        vec_t s1 = RROT(msg[(i + 14) % MSG_SIZE], 17) ^ RROT(msg[(i + 14) % MSG_SIZE], 19) ^ (msg[(i + 14) % MSG_SIZE] >> 10);
        msg[i % MSG_SIZE] = msg[i % MSG_SIZE] + s0 + msg[(i + 9) % MSG_SIZE] + s1;

        s1 = RROT(hash[(4 + 7 * i) % 8], 6) ^ RROT(hash[(4 + 7 * i) % 8], 11) ^ RROT(hash[(4 + 7 * i) % 8], 25);
        vec_t ch = (hash[(4 + 7 * i) % 8] & hash[(5 + 7 * i) % 8]) ^ ((~hash[(4 + 7 * i) % 8]) & hash[(6 + 7 * i) % 8]);
        vec_t temp1 = hash[(7 + 7 * i) % 8] + s1 + ch + k[i] + msg[i % MSG_SIZE];
        s0 = RROT(hash[(0 + 7 * i) % 8], 2) ^ RROT(hash[(0 + 7 * i) % 8], 13) ^ RROT(hash[(0 + 7 * i) % 8], 22);
        vec_t maj = (hash[(0 + 7 * i) % 8] & hash[(1 + 7 * i) % 8]) ^ (hash[(0 + 7 * i) % 8] & hash[(2 + 7 * i) % 8]) ^ (hash[(1 + 7 * i) % 8] & hash[(2 + 7 * i) % 8]);
        vec_t temp2 = s0 + maj;
        hash[(3 + 7 * i) % 8] = hash[(3 + 7 * i) % 8] + temp1;
        hash[(7 + 7 * i) % 8] = temp1 + temp2;
    }
}

void sha256_init_msg(vec_t msg[MSG_SIZE])
{
#pragma GCC unroll 128
    for (int i = 0; i < VEC_SIZE; i++) {
        msg[2][i] = 0x80000000;
        for (int j = 3; j < 15; j++)
            msg[j][i] = 0;
        msg[15][i] = sizeof(uint64_t) * 8;
    }
}

void sha256_load_input(vec_t msg[MSG_SIZE], const vec64_t* input)
{
#pragma GCC unroll 128
    for (int i = 0; i < VEC_SIZE; i++) {
        uint64_t le_input = htole64(((*input)[i]));
        uint32_t lower, upper;
        memcpy(&lower, &le_input, 4);
        memcpy(&upper, ((char*)&le_input) + 4, 4);
        msg[0][i] = be32toh(lower);
        msg[1][i] = be32toh(upper);
    }
}

void hash_preprocess(uint8_t hash[SHA256_LEN])
{
    uint32_t* h32 = (uint32_t*)hash;
    h32[0] = be32toh(h32[0]) - 0x6a09e667;
    h32[1] = be32toh(h32[1]) - 0xbb67ae85;
    h32[2] = be32toh(h32[2]) - 0x3c6ef372;
    h32[3] = be32toh(h32[3]) - 0xa54ff53a;
    h32[4] = be32toh(h32[4]) - 0x510e527f;
    h32[5] = be32toh(h32[5]) - 0x9b05688c;
    h32[6] = be32toh(h32[6]) - 0x1f83d9ab;
    h32[7] = be32toh(h32[7]) - 0x5be0cd19;
}

void hash_to_str(char* str, uint8_t hash[SHA256_LEN])
{
    for (int i = 0; i < SHA256_LEN; i++)
        sprintf(str + i * 2, "%02x", hash[i]);
}

void sha256_openssl(uint64_t input, uint8_t hash[SHA256_LEN])
{
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, &input, 8);
    SHA256_Final(hash, &ctx);
}