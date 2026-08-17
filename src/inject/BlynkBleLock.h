/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BlynkBleLock_h
#define BlynkBleLock_h

#include <mutex>

/*
 * Bare-metal toolchains (i.e. arm-none-eabi, used for SAMD) ship a libstdc++
 * built without threading support, so std::mutex is not available there.
 * On such platforms we fall back to a no-op lock (the BLE stack runs
 * in the application context anyway).
 */
#if defined(_GLIBCXX_HAS_GTHREADS) || defined(_LIBCPP_VERSION)
typedef std::mutex BlynkBleLock;
#else
class BlynkBleLock {
public:
    void lock()   {}
    void unlock() {}
};
#endif

#endif /* BlynkBleLock_h */
