/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ESP8266 has no crypto accelerator, but the core already ships BearSSL,
 * so reusing its hashes keeps the footprint down.
 */

#pragma once

class DigestEngineSHA256 {
public:
  static const size_t DIGEST_SIZE = br_sha256_SIZE;
  static const size_t BLOCK_SIZE  = 64;

  DigestEngineSHA256() { reset(); }

  unsigned getDigestSize() const { return DIGEST_SIZE; }

  void reset() { br_sha256_init(&m_ctx); }

  void update(const void* data, size_t len) { br_sha256_update(&m_ctx, data, len); }

  void update(uint8_t b) { update(&b, 1); }

  /*
   * Writes DIGEST_SIZE bytes into result.
   * Does not modify the engine, so it can be called multiple times,
   * as well as followed by more update() calls.
   */
  void getDigestBuffer(uint8_t* result) const { br_sha256_out(&m_ctx, result); }

private:
  br_sha256_context m_ctx;
};

class DigestEngineMD5 {
public:
  static const size_t DIGEST_SIZE = br_md5_SIZE;
  static const size_t BLOCK_SIZE  = 64;

  DigestEngineMD5() { reset(); }

  unsigned getDigestSize() const { return DIGEST_SIZE; }

  void reset() { br_md5_init(&m_ctx); }

  void update(const void* data, size_t len) { br_md5_update(&m_ctx, data, len); }

  void update(uint8_t b) { update(&b, 1); }

  /*
   * Writes DIGEST_SIZE bytes into result.
   * Does not modify the engine, so it can be called multiple times,
   * as well as followed by more update() calls.
   */
  void getDigestBuffer(uint8_t* result) const { br_md5_out(&m_ctx, result); }

private:
  br_md5_context m_ctx;
};
