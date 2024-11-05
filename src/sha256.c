#include "sha256.h"
#include <stdio.h>

#include <endian.h>
#include <immintrin.h>
#include <memory.h>

// Reference: https://en.wikipedia.org/wiki/SHA-2
//            https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=sha2

// SHA256 hashing function optimised for a 64-bit input size and uses SIMD.
// Relies heavily on loop unrolling. Uses SHA256 SIMD instructions that may
// or may not be present ¯\_(ツ)_/¯.
void sha256_simd(uint64_t input, uint8_t hash[SHA256_LEN])
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

    // Prepare input digest
    uint8_t w[64];
    memcpy(w, &input, 8);
    w[8] = 0x80;
    memset(w + 9, 0, 47);
    uint64_t sz = htobe64(sizeof(uint64_t) * 8);
    memcpy(w + 56, &sz, 8);

    // Prepare start state
    __m128i s0 = _mm_set_epi32(0x6a09e667, 0xbb67ae85, 0x510e527f, 0x9b05688c); // a b e f
    __m128i s1 = _mm_set_epi32(0x3c6ef372, 0xa54ff53a, 0x1f83d9ab, 0x5be0cd19); // c d g h

    __m128i msg[4];

    // The first four rounds are slightly different and it doesn't seem like the compiler is smart
    // enough to optimise it properly if they are put in the main loop with extra if-statements.
    for (int i = 0; i < 4; i++) {
        msg[i] = _mm_shuffle_epi8(
            _mm_loadu_si128((__m128i*)(w + i * 16)),
            _mm_set_epi32(0x0c0d0e0f, 0x08090a0b, 0x04050607, 0x00010203));
        if (i == 3)
            msg[0] = _mm_sha256msg2_epu32(_mm_add_epi32(msg[0], _mm_alignr_epi8(msg[3], msg[2], 4)), msg[3]);
        if (i != 0)
            msg[i - 1] = _mm_sha256msg1_epu32(msg[i - 1], msg[i]);

        __m128i ks = _mm_add_epi32(msg[i], _mm_set_epi32(k[3 + 4 * i], k[2 + 4 * i], k[1 + 4 * i], k[0 + 4 * i]));
        s1 = _mm_sha256rnds2_epu32(s1, s0, ks);

        ks = _mm_shuffle_epi32(ks, 0x0E);
        s0 = _mm_sha256rnds2_epu32(s0, s1, ks);
    }

    // The 'main' loop for rounds 4 to 15
    for (int i = 4; i < 16; i++) {
        // These if statements aren't necessary, and the compiler is probably smart enough to do the
        // optimisation that they try to achieve even if they aren't present.
        if (i < 15) {
            msg[(i + 1) % 4] = _mm_add_epi32(msg[(i + 1) % 4], _mm_alignr_epi8(msg[i % 4], msg[(i + 3) % 4], 4));
            msg[(i + 1) % 4] = _mm_sha256msg2_epu32(msg[(i + 1) % 4], msg[i % 4]);
        }
        if (i < 13)
            msg[(i + 3) % 4] = _mm_sha256msg1_epu32(msg[(i + 3) % 4], msg[i % 4]);

        __m128i ks = _mm_add_epi32(msg[i % 4], _mm_set_epi32(k[3 + 4 * i], k[2 + 4 * i], k[1 + 4 * i], k[0 + 4 * i]));
        s1 = _mm_sha256rnds2_epu32(s1, s0, ks);

        ks = _mm_shuffle_epi32(ks, 0x0E);
        s0 = _mm_sha256rnds2_epu32(s0, s1, ks);
    }

    // Shuffle the integers back
    __m128i tmp1 = _mm_shuffle_epi32(s0, 0b00011011); // f e b a
    __m128i tmp2 = _mm_shuffle_epi32(s1, 0b10110001); // d c h g
    s0 = _mm_blend_epi16(tmp1, tmp2, 0b11110000);     // d c b a
    s1 = _mm_alignr_epi8(tmp2, tmp1, 0b00001000);     // h g f e

    // Store result
    _mm_storeu_si128((__m128i*)&hash[0], s0);
    _mm_storeu_si128((__m128i*)&hash[16], s1);
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