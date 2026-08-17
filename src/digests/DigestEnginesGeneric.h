/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal, dependency-free implementation,
 * used on platforms that don't provide a hardware/system digest engine.
 */

#pragma once

/*
 * SHA-256 (FIPS 180-4)
 */
class DigestEngineSHA256 {
public:
    static const size_t DIGEST_SIZE = 32;
    static const size_t BLOCK_SIZE = 64;

    DigestEngineSHA256() { reset(); }

    unsigned getDigestSize() const { return DIGEST_SIZE; }

    void reset() {
        m_length = 0;
        m_buffLen = 0;
        m_state[0] = 0x6a09e667;
        m_state[1] = 0xbb67ae85;
        m_state[2] = 0x3c6ef372;
        m_state[3] = 0xa54ff53a;
        m_state[4] = 0x510e527f;
        m_state[5] = 0x9b05688c;
        m_state[6] = 0x1f83d9ab;
        m_state[7] = 0x5be0cd19;
    }

    void update(const void* data, size_t len) {
        const uint8_t* p = (const uint8_t*)data;
        while (len) {
            const size_t avail = BLOCK_SIZE - m_buffLen;
            const size_t chunk = (len < avail) ? len : avail;
            memcpy(m_buff + m_buffLen, p, chunk);
            m_buffLen += chunk;
            m_length += chunk;
            p += chunk;
            len -= chunk;
            if (m_buffLen == BLOCK_SIZE) {
                transform(m_buff);
                m_buffLen = 0;
            }
        }
    }

    void update(uint8_t b) { update(&b, 1); }

  /*
   * Writes DIGEST_SIZE bytes into result.
   * Does not modify the engine, so it can be called multiple times,
   * as well as followed by more update() calls.
   */
    void getDigestBuffer(uint8_t* result) const {
        DigestEngineSHA256 tmp(*this);
        tmp.finalize(result);
    }

private:
    void finalize(uint8_t* result) {
        const uint64_t bits = uint64_t(m_length) * 8;

        update(uint8_t(0x80));
        while (m_buffLen != BLOCK_SIZE - 8) {
            update(uint8_t(0x00));
        }
        for (int i = 7; i >= 0; i--) {
            update(uint8_t(bits >> (i * 8)));
        }

        for (int i = 0; i < 8; i++) {
            result[i * 4 + 0] = uint8_t(m_state[i] >> 24);
            result[i * 4 + 1] = uint8_t(m_state[i] >> 16);
            result[i * 4 + 2] = uint8_t(m_state[i] >> 8);
            result[i * 4 + 3] = uint8_t(m_state[i]);
        }
    }

    static uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void transform(const uint8_t* block) {
        static const uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
            0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
            0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
            0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
            0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = (uint32_t(block[i * 4 + 0]) << 24) |
                   (uint32_t(block[i * 4 + 1]) << 16) |
                   (uint32_t(block[i * 4 + 2]) << 8) |
                   (uint32_t(block[i * 4 + 3]));
        }
        for (int i = 16; i < 64; i++) {
            const uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];

        for (int i = 0; i < 64; i++) {
            const uint32_t S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25);
            const uint32_t ch = (e & f) ^ ((~e) & g);
            const uint32_t temp1 = h + S1 + ch + K[i] + w[i];
            const uint32_t S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t temp2 = S0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
        m_state[5] += f;
        m_state[6] += g;
        m_state[7] += h;
    }

    uint32_t m_state[8];
    uint64_t m_length;
    size_t m_buffLen;
    uint8_t m_buff[BLOCK_SIZE];
};
