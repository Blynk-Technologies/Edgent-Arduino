/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 * ESP32 uses MBEDTLS_SHA256_ALT, i.e. bound to the on-chip SHA accelerator
 * (with a transparent software fallback while the engine is busy).
 * 
 * MD5 is not accelerated, but reusing the system
 * implementation avoids pulling in a second copy of the algorithm.
 */

#pragma once

class DigestEngineSHA256 {
public:
  static const size_t DIGEST_SIZE = 32;
  static const size_t BLOCK_SIZE  = 64;

  DigestEngineSHA256() {
    mbedtls_sha256_init(&m_ctx);
    ctxStarts(&m_ctx);
  }

  DigestEngineSHA256(const DigestEngineSHA256& other) {
    mbedtls_sha256_init(&m_ctx);
    mbedtls_sha256_clone(&m_ctx, &other.m_ctx);
  }

  DigestEngineSHA256& operator=(const DigestEngineSHA256& other) {
    if (this != &other) {
      mbedtls_sha256_free(&m_ctx);
      mbedtls_sha256_init(&m_ctx);
      mbedtls_sha256_clone(&m_ctx, &other.m_ctx);
    }
    return *this;
  }

  ~DigestEngineSHA256() {
    mbedtls_sha256_free(&m_ctx);
  }

  unsigned getDigestSize() const { return DIGEST_SIZE; }

  void reset() {
    mbedtls_sha256_free(&m_ctx);
    mbedtls_sha256_init(&m_ctx);
    ctxStarts(&m_ctx);
  }

  void update(const void* data, size_t len) {
    ctxUpdate(&m_ctx, (const unsigned char*)data, len);
  }

  void update(uint8_t b) { update(&b, 1); }

  /*
   * Writes DIGEST_SIZE bytes into result.
   * Does not modify the engine, so it can be called multiple times,
   * as well as followed by more update() calls.
   */
  void getDigestBuffer(uint8_t* result) const {
    mbedtls_sha256_context tmp;
    mbedtls_sha256_init(&tmp);
    mbedtls_sha256_clone(&tmp, &m_ctx);
    ctxFinish(&tmp, result);
    mbedtls_sha256_free(&tmp);
  }

private:
  /*
   * mbedTLS 2.x deprecated the void-returning entry points in favour of
   * the *_ret() ones; mbedTLS 3.x dropped the *_ret() names again.
   */
  static void ctxStarts(mbedtls_sha256_context* ctx) {
#if MBEDTLS_VERSION_NUMBER < 0x03000000
    mbedtls_sha256_starts_ret(ctx, 0);
#else
    mbedtls_sha256_starts(ctx, 0);
#endif
  }

  static void ctxUpdate(mbedtls_sha256_context* ctx, const unsigned char* data, size_t len) {
#if MBEDTLS_VERSION_NUMBER < 0x03000000
    mbedtls_sha256_update_ret(ctx, data, len);
#else
    mbedtls_sha256_update(ctx, data, len);
#endif
  }

  static void ctxFinish(mbedtls_sha256_context* ctx, unsigned char* result) {
#if MBEDTLS_VERSION_NUMBER < 0x03000000
    mbedtls_sha256_finish_ret(ctx, result);
#else
    mbedtls_sha256_finish(ctx, result);
#endif
  }

  mbedtls_sha256_context m_ctx;
};

class DigestEngineMD5 {
public:
  static const size_t DIGEST_SIZE = 16;
  static const size_t BLOCK_SIZE  = 64;

  DigestEngineMD5() {
    mbedtls_md5_init(&m_ctx);
    ctxStarts(&m_ctx);
  }

  DigestEngineMD5(const DigestEngineMD5& other) {
    mbedtls_md5_init(&m_ctx);
    mbedtls_md5_clone(&m_ctx, &other.m_ctx);
  }

  DigestEngineMD5& operator=(const DigestEngineMD5& other) {
    if (this != &other) {
      mbedtls_md5_free(&m_ctx);
      mbedtls_md5_init(&m_ctx);
      mbedtls_md5_clone(&m_ctx, &other.m_ctx);
    }
    return *this;
  }

  ~DigestEngineMD5() {
    mbedtls_md5_free(&m_ctx);
  }

  unsigned getDigestSize() const { return DIGEST_SIZE; }

  void reset() {
    mbedtls_md5_free(&m_ctx);
    mbedtls_md5_init(&m_ctx);
    ctxStarts(&m_ctx);
  }

  void update(const void* data, size_t len) {
    ctxUpdate(&m_ctx, (const unsigned char*)data, len);
  }

  void update(uint8_t b) { update(&b, 1); }

  /*
   * Writes DIGEST_SIZE bytes into result.
   * Does not modify the engine, so it can be called multiple times,
   * as well as followed by more update() calls.
   */
  void getDigestBuffer(uint8_t* result) const {
    mbedtls_md5_context tmp;
    mbedtls_md5_init(&tmp);
    mbedtls_md5_clone(&tmp, &m_ctx);
    ctxFinish(&tmp, result);
    mbedtls_md5_free(&tmp);
  }

private:
  static void ctxStarts(mbedtls_md5_context* ctx) {
#if MBEDTLS_VERSION_NUMBER < 0x03000000
    mbedtls_md5_starts_ret(ctx);
#else
    mbedtls_md5_starts(ctx);
#endif
  }

  static void ctxUpdate(mbedtls_md5_context* ctx, const unsigned char* data, size_t len) {
#if MBEDTLS_VERSION_NUMBER < 0x03000000
    mbedtls_md5_update_ret(ctx, data, len);
#else
    mbedtls_md5_update(ctx, data, len);
#endif
  }

  static void ctxFinish(mbedtls_md5_context* ctx, unsigned char* result) {
#if MBEDTLS_VERSION_NUMBER < 0x03000000
    mbedtls_md5_finish_ret(ctx, result);
#else
    mbedtls_md5_finish(ctx, result);
#endif
  }

  mbedtls_md5_context m_ctx;
};
