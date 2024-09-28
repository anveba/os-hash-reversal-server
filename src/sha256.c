#include "sha256.h"
#include <stdio.h>

#include <endian.h>

// Reference: https://en.wikipedia.org/wiki/SHA-2

#define RROT(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

// SHA256 hashing function optimised for a 64-bit input size.
void sha256(uint64_t input, uint8_t hash[SHA256_LEN])
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

    const uint32_t h0 = 0x6a09e667;
    const uint32_t h1 = 0xbb67ae85;
    const uint32_t h2 = 0x3c6ef372;
    const uint32_t h3 = 0xa54ff53a;
    const uint32_t h4 = 0x510e527f;
    const uint32_t h5 = 0x9b05688c;
    const uint32_t h6 = 0x1f83d9ab;
    const uint32_t h7 = 0x5be0cd19;

    // Prepare first 512-bit chunk with proper endianness
    uint32_t w[64];
    w[0] = be32toh(((uint32_t*)&input)[0]);
    w[1] = be32toh(((uint32_t*)&input)[1]);
    w[2] = 0x80000000;
    w[15] = sizeof(uint64_t) * 8;
    for (int i = 3; i < 15; i++)
        w[i] = 0;

    // Fill the rest of the message schedule array (w)
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = RROT(w[i - 15], 7) ^ RROT(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = RROT(w[i - 2], 17) ^ RROT(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;

    // Main loop
    for (int i = 0; i < 64; i++) {
        uint32_t s1 = RROT(e, 6) ^ RROT(e, 11) ^ RROT(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + k[i] + w[i];
        uint32_t s0 = RROT(a, 2) ^ RROT(a, 13) ^ RROT(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    // Complete final hash (with proper endianness)
    ((uint32_t*)hash)[0] = htobe32(a + h0);
    ((uint32_t*)hash)[1] = htobe32(b + h1);
    ((uint32_t*)hash)[2] = htobe32(c + h2);
    ((uint32_t*)hash)[3] = htobe32(d + h3);
    ((uint32_t*)hash)[4] = htobe32(e + h4);
    ((uint32_t*)hash)[5] = htobe32(f + h5);
    ((uint32_t*)hash)[6] = htobe32(g + h6);
    ((uint32_t*)hash)[7] = htobe32(h + h7);
}

void hash_to_str(char* str, uint8_t hash[SHA256_LEN])
{
    for (int i = 0; i < SHA256_LEN; i++)
        sprintf(str + i * 2, "%02x", hash[i]);
}