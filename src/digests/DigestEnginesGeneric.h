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
  static const size_t BLOCK_SIZE  = 64;

  DigestEngineSHA256() { reset(); }

  unsigned getDigestSize() const { return DIGEST_SIZE; }

  void reset() {
    m_length  = 0;
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
      m_length  += chunk;
      p         += chunk;
      len       -= chunk;
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
      update(uint8_t(bits >> (i*8)));
    }

    for (int i = 0; i < 8; i++) {
      result[i*4 + 0] = uint8_t(m_state[i] >> 24);
      result[i*4 + 1] = uint8_t(m_state[i] >> 16);
      result[i*4 + 2] = uint8_t(m_state[i] >> 8);
      result[i*4 + 3] = uint8_t(m_state[i]);
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
      w[i] = (uint32_t(block[i*4 + 0]) << 24) |
             (uint32_t(block[i*4 + 1]) << 16) |
             (uint32_t(block[i*4 + 2]) << 8)  |
             (uint32_t(block[i*4 + 3]));
    }
    for (int i = 16; i < 64; i++) {
      const uint32_t s0 = ror(w[i-15], 7)  ^ ror(w[i-15], 18) ^ (w[i-15] >> 3);
      const uint32_t s1 = ror(w[i-2], 17)  ^ ror(w[i-2], 19)  ^ (w[i-2] >> 10);
      w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
    uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];

    for (int i = 0; i < 64; i++) {
      const uint32_t S1    = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25);
      const uint32_t ch    = (e & f) ^ ((~e) & g);
      const uint32_t temp1 = h + S1 + ch + K[i] + w[i];
      const uint32_t S0    = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22);
      const uint32_t maj   = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t temp2 = S0 + maj;

      h = g; g = f; f = e; e = d + temp1;
      d = c; c = b; b = a; a = temp1 + temp2;
    }

    m_state[0] += a; m_state[1] += b; m_state[2] += c; m_state[3] += d;
    m_state[4] += e; m_state[5] += f; m_state[6] += g; m_state[7] += h;
  }

  uint32_t m_state[8];
  uint64_t m_length;
  size_t   m_buffLen;
  uint8_t  m_buff[BLOCK_SIZE];
};

/*
 * MD5 (RFC 1321)
 */
class DigestEngineMD5 {
public:
  static const size_t DIGEST_SIZE = 16;
  static const size_t BLOCK_SIZE  = 64;

  DigestEngineMD5() { reset(); }

  unsigned getDigestSize() const { return DIGEST_SIZE; }

  void reset() {
    m_length  = 0;
    m_buffLen = 0;
    m_state[0] = 0x67452301;
    m_state[1] = 0xefcdab89;
    m_state[2] = 0x98badcfe;
    m_state[3] = 0x10325476;
  }

  void update(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    while (len) {
      const size_t avail = BLOCK_SIZE - m_buffLen;
      const size_t chunk = (len < avail) ? len : avail;
      memcpy(m_buff + m_buffLen, p, chunk);
      m_buffLen += chunk;
      m_length  += chunk;
      p         += chunk;
      len       -= chunk;
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
    DigestEngineMD5 tmp(*this);
    tmp.finalize(result);
  }

private:
  void finalize(uint8_t* result) {
    const uint64_t bits = uint64_t(m_length) * 8;

    update(uint8_t(0x80));
    while (m_buffLen != BLOCK_SIZE - 8) {
      update(uint8_t(0x00));
    }
    for (int i = 0; i < 8; i++) {
      update(uint8_t(bits >> (i*8)));
    }

    for (int i = 0; i < 4; i++) {
      result[i*4 + 0] = uint8_t(m_state[i]);
      result[i*4 + 1] = uint8_t(m_state[i] >> 8);
      result[i*4 + 2] = uint8_t(m_state[i] >> 16);
      result[i*4 + 3] = uint8_t(m_state[i] >> 24);
    }
  }

  static uint32_t rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

  void transform(const uint8_t* block) {
    static const uint32_t K[64] = {
      0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
      0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
      0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
      0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
      0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
      0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
      0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
      0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
      0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
      0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
      0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
      0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
      0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
      0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
      0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
      0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };

    static const uint8_t S[64] = {
      7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
      5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
      4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
      6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
    };

    uint32_t w[16];
    for (int i = 0; i < 16; i++) {
      w[i] = (uint32_t(block[i*4 + 0]))       |
             (uint32_t(block[i*4 + 1]) << 8)  |
             (uint32_t(block[i*4 + 2]) << 16) |
             (uint32_t(block[i*4 + 3]) << 24);
    }

    uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];

    for (int i = 0; i < 64; i++) {
      uint32_t f;
      int g;
      if (i < 16) {
        f = (b & c) | (~b & d);
        g = i;
      } else if (i < 32) {
        f = (d & b) | (~d & c);
        g = (5*i + 1) % 16;
      } else if (i < 48) {
        f = b ^ c ^ d;
        g = (3*i + 5) % 16;
      } else {
        f = c ^ (b | ~d);
        g = (7*i) % 16;
      }

      f = f + a + K[i] + w[g];
      a = d; d = c; c = b;
      b += rol(f, S[i]);
    }

    m_state[0] += a; m_state[1] += b; m_state[2] += c; m_state[3] += d;
  }

  uint32_t m_state[4];
  uint64_t m_length;
  size_t   m_buffLen;
  uint8_t  m_buff[BLOCK_SIZE];
};
