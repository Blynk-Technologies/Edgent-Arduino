/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * NimBLE bundles TinyCrypt, so on platforms that have no system digest engine
 * of their own, reusing it avoids a second copy of SHA-256.
 */

#pragma once

class DigestEngineSHA256 {
public:
    static const size_t DIGEST_SIZE = TC_SHA256_DIGEST_SIZE;
    static const size_t BLOCK_SIZE = TC_SHA256_BLOCK_SIZE;

    DigestEngineSHA256() { reset(); }

    unsigned getDigestSize() const { return DIGEST_SIZE; }

    void reset() { tc_sha256_init(&m_ctx); }

    void update(const void* data, size_t len) {
        tc_sha256_update(&m_ctx, (const uint8_t*)data, len);
    }

    void update(uint8_t b) { update(&b, 1); }

  /*
   * Writes DIGEST_SIZE bytes into result.
   * Does not modify the engine, so it can be called multiple times,
   * as well as followed by more update() calls.
   */
    void getDigestBuffer(uint8_t* result) const {
        tc_sha256_state_struct tmp = m_ctx;
        tc_sha256_final(result, &tmp);
    }

private:
    tc_sha256_state_struct m_ctx;
};
