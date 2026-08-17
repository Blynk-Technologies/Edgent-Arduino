/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined(ESP32)
  #include <mbedtls/md.h>
  #include <digests/DigestEnginesMbedTLS.h>
#elif defined(ESP8266)
  #include <bearssl/bearssl_hash.h>
  #include <digests/DigestEnginesBearSSL.h>
#elif __has_include(<nimble/ext/tinycrypt/include/tinycrypt/sha256.h>)
  #include <nimble/ext/tinycrypt/include/tinycrypt/sha256.h>
  #include <digests/DigestEnginesTinyCrypt.h>
#elif __has_include(<Crypto.h>) && __has_include(<SHA256.h>)
  //#include <SHA1.h> // CryptoLegacy
  #include <SHA256.h>
  #include <digests/DigestEnginesArduinoCrypto.h>
#else
  #include <digests/DigestEnginesGeneric.h>
#endif

class DigestEngineCRC32 {

public:
    static const size_t DIGEST_SIZE = 4;
    static const size_t BLOCK_SIZE = 1;

    DigestEngineCRC32() {
        reset();
    }

    void reset() {
        _state = ~0L;
    }

    void update(const uint8_t& data) {
        static const uint32_t table[16] = {
            0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
            0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
            0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
            0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
        };
        uint8_t tbl_idx;
        tbl_idx = _state ^ (data >> (0 * 4));
        _state = (*(table + (tbl_idx & 0x0f))) ^ (_state >> 4);
        tbl_idx = _state ^ (data >> (1 * 4));
        _state = (*(table + (tbl_idx & 0x0f))) ^ (_state >> 4);
    }

    void update(const void* buffer, size_t len) {
        const uint8_t* data = (const uint8_t*)buffer;
        while (len--) {
            update(*data++);
        }
    }

    unsigned getDigestSize() const {
        return DIGEST_SIZE;
    }

    void getDigestBuffer(uint8_t* buffer) const {
        uint32_t result = ~_state;
        buffer[3] = (result >> 0) & 0xFF;
        buffer[2] = (result >> 8) & 0xFF;
        buffer[1] = (result >> 16) & 0xFF;
        buffer[0] = (result >> 24) & 0xFF;
    }

private:
    uint32_t _state;
};


template <typename T>
String getDigestBase64(const T& md) {
    const unsigned len = T::DIGEST_SIZE;
    uint8_t buffer[len];
    uint8_t* pos;
    const uint8_t *end, *in;
    const unsigned olen = len * 4 / 3 + 4 + 1; /* 3-byte blocks to 4-byte + nul */
    uint8_t output[olen];

    static const uint8_t base64_table[65] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    md.getDigestBuffer(buffer);

    end = buffer + len;
    in = buffer;
    pos = output;
    while (end - in >= 3) {
        *pos++ = base64_table[in[0] >> 2];
        *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = base64_table[in[2] & 0x3f];
        in += 3;
    }

    if (end - in) {
        *pos++ = base64_table[in[0] >> 2];
        if (end - in == 1) {
            *pos++ = base64_table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        } else {
            *pos++ = base64_table[((in[0] & 0x03) << 4) |
                                  (in[1] >> 4)];
            *pos++ = base64_table[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
    }

    *pos = '\0';

    return String((const char*)output);
}

template <typename T>
String getDigestHex(const T& md) {
    const unsigned len = T::DIGEST_SIZE;
    uint8_t buffer[len];
    char output[(len * 2) + 1];
    md.getDigestBuffer(buffer);
    for (unsigned i = 0; i < len; i++) {
        sprintf(output + (i * 2), "%02x", buffer[i]);
    }
    return String(output);
}
