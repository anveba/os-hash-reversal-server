#include "sha256.h"
#include <stdio.h>

#include <endian.h>
#include <immintrin.h>
#include <memory.h>

// Reference: https://en.wikipedia.org/wiki/SHA-2
//            https://gcc.gnu.org/onlinedocs/gcc/Vector-Extensions.html

#define RROT(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

// SHA256 hashing function optimised for a 64-bit input size and uses SIMD.
// Specialised SHA256 SIMD instructions exist on some architectures, but not
// all, so this implementation does not use them. This function does not
// compute the final bit of the SHA256 hash, as this is reversable.
void sha256_simd(uint64_t input[VEC_SIZE], vec_t hash[OUTPUT_VECS])
{
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

    const uint32_t hs[8] = { 0x6a09e667,
                             0xbb67ae85,
                             0x3c6ef372,
                             0xa54ff53a,
                             0x510e527f,
                             0x9b05688c,
                             0x1f83d9ab,
                             0x5be0cd19 };

    // Prepare first 512-bit chunk with proper endianness
    vec_t msg[16];
    for (int i = 0; i < VEC_SIZE; i++) {
        msg[0][i] = be32toh(((uint32_t*)(input + i))[0]);
        msg[1][i] = be32toh(((uint32_t*)(input + i))[1]);
        msg[2][i] = 0x80000000;
        msg[15][i] = sizeof(uint64_t) * 8;
        for (int j = 3; j < 15; j++)
            msg[j][i] = 0;
    }

    // Prepare the initial state
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

    // Main loop
    for (int i = 0; i < 64; i++) {
        if (i >= 16) {
            vec_t s0 = RROT(msg[(i + 1) % 16], 7) ^ RROT(msg[(i + 1) % 16], 18) ^ (msg[(i + 1) % 16] >> 3);
            vec_t s1 = RROT(msg[(i + 14) % 16], 17) ^ RROT(msg[(i + 14) % 16], 19) ^ (msg[(i + 14) % 16] >> 10);
            msg[i % 16] = msg[i % 16] + s0 + msg[(i + 9) % 16] + s1;
        }

        vec_t s1 = RROT(hash[4], 6) ^ RROT(hash[4], 11) ^ RROT(hash[4], 25);
        vec_t ch = (hash[4] & hash[5]) ^ ((~hash[4]) & hash[6]);
        vec_t temp1 = hash[7] + s1 + ch + k[i] + msg[i % 16];
        vec_t s0 = RROT(hash[0], 2) ^ RROT(hash[0], 13) ^ RROT(hash[0], 22);
        vec_t maj = (hash[0] & hash[1]) ^ (hash[0] & hash[2]) ^ (hash[1] & hash[2]);
        vec_t temp2 = s0 + maj;
        hash[7] = hash[6];
        hash[6] = hash[5];
        hash[5] = hash[4];
        hash[4] = hash[3] + temp1;
        hash[3] = hash[2];
        hash[2] = hash[1];
        hash[1] = hash[0];
        hash[0] = temp1 + temp2;
    }
}

void hash_prepare_for_simd(uint8_t hash[SHA256_LEN])
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