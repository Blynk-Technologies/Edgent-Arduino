/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Arduino Cryptography Library (rweather/arduinolibs).
 * Its hashes expose the sizes as virtual methods only,
 * so they are also passed as template arguments.
 */

#pragma once

template<typename T, unsigned DS, unsigned BS>
class DigestEngineArduinoCrypto {
public:
  static const size_t DIGEST_SIZE = DS;
  static const size_t BLOCK_SIZE  = BS;

  unsigned getDigestSize() const { return DIGEST_SIZE; }

  void reset() { m_md.clear(); }

  void update(const void* data, size_t len) { m_md.update(data, len); }

  void update(uint8_t b) { update(&b, 1); }

  /*
   * Writes DIGEST_SIZE bytes into result.
   * Does not modify the engine, so it can be called multiple times,
   * as well as followed by more update() calls.
   */
  void getDigestBuffer(uint8_t* result) const {
    T copy = m_md;
    copy.finalize(result, DIGEST_SIZE);
  }

private:
  T m_md;
};

//typedef DigestEngineArduinoCrypto<SHA1,   20, 64> DigestEngineSHA1;
typedef DigestEngineArduinoCrypto<SHA256, 32, 64> DigestEngineSHA256;
