/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ESP32 uses MBEDTLS_SHA256_ALT, i.e. bound to the on-chip SHA accelerator
 * (with a transparent software fallback while the engine is busy).
 *
 * The generic mbedtls_md_* API is used, as it dispatches to the same
 * (possibly accelerated) primitives, while keeping the same function
 * names across mbedTLS 2.x and 3.x.
 */

#pragma once

template <unsigned MD, unsigned DS, unsigned BS>
class DigestEngineMbedTLS {
public:
    static const size_t DIGEST_SIZE = DS;
    static const size_t BLOCK_SIZE = BS;

    DigestEngineMbedTLS() {
        setup();
        mbedtls_md_starts(&m_ctx);
    }

    DigestEngineMbedTLS(const DigestEngineMbedTLS& other) {
        setup();
        mbedtls_md_clone(&m_ctx, &other.m_ctx);
    }

    DigestEngineMbedTLS& operator=(const DigestEngineMbedTLS& other) {
        if (this != &other) {
            mbedtls_md_clone(&m_ctx, &other.m_ctx);
        }
        return *this;
    }

    ~DigestEngineMbedTLS() {
        mbedtls_md_free(&m_ctx);
    }

    unsigned getDigestSize() const { return DIGEST_SIZE; }

    void reset() {
        mbedtls_md_starts(&m_ctx);
    }

    void update(const void* data, size_t len) {
        mbedtls_md_update(&m_ctx, (const unsigned char*)data, len);
    }

    void update(uint8_t b) { update(&b, 1); }

  /*
   * Writes DIGEST_SIZE bytes into result.
   * Does not modify the engine, so it can be called multiple times,
   * as well as followed by more update() calls.
   */
    void getDigestBuffer(uint8_t* result) const {
        mbedtls_md_context_t tmp;
        mbedtls_md_init(&tmp);
        mbedtls_md_setup(&tmp, mbedtls_md_info_from_type((mbedtls_md_type_t)MD), 0);
        mbedtls_md_clone(&tmp, &m_ctx);
        mbedtls_md_finish(&tmp, result);
        mbedtls_md_free(&tmp);
    }

private:
    void setup() {
        mbedtls_md_init(&m_ctx);
        mbedtls_md_setup(&m_ctx, mbedtls_md_info_from_type((mbedtls_md_type_t)MD), 0);
    }

    mbedtls_md_context_t m_ctx;
};

typedef DigestEngineMbedTLS<MBEDTLS_MD_SHA256, 32, 64> DigestEngineSHA256;
//typedef DigestEngineMbedTLS<MBEDTLS_MD_MD5,    16, 64> DigestEngineMD5;
