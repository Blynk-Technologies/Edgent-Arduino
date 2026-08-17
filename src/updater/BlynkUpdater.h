/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BlynkUpdater_h
#define BlynkUpdater_h

#include <digests/DigestEngines.h>
#include <NetMgrLogger.h>

/*
 * Drives the update process, leaving the actual storage access
 * to the platform-specific subclass.
 * The platform Update libraries verify MD5 at best, so the SHA256 of the
 * incoming image is computed here on the fly, and checked before committing.
 */
class BlynkUpdaterBase {
public:

    virtual ~BlynkUpdaterBase() {}

    // Starts an update. size 0 means "use all the available space"
    bool begin(size_t size = 0) {
        if (!doBegin(size)) {
            return false;
        }
        reset();
        return true;
    }

    // Expected digest of the whole image, base64-encoded
    bool setSHA256(const char* expected_sha256) {
        if (_written) {
            LOG_E("setSHA256() must be called before any write()");
            return false;
        }
        String sha(expected_sha256 ? expected_sha256 : "");
        sha.trim();
        if (sha.length() != ((DigestEngineSHA256::DIGEST_SIZE + 2) / 3) * 4) {
            return false;
        }
        _expected_sha = sha;
        _sha.reset();
        return true;
    }

    bool write(const uint8_t* buff, unsigned len) {
        if (!doWrite(buff, len)) {
            return false;
        }
        if (_expected_sha.length()) {
            _sha.update(buff, len);
        }
        _written += len;
        return true;
    }

    // Verifies the image and commits it
    bool end() {
        if (_expected_sha.length()) {
            const String actual = getDigestBase64(_sha);
            if (actual != _expected_sha) {
                LOG_E("SHA256 mismatch (expected: %s, actual: %s)",
                      _expected_sha.c_str(), actual.c_str());
                abort();
                return false;
            }
        }
        return doEnd();
    }

    void abort() {
        doAbort();
        reset();
    }

    // SHA256 of the data written so far (empty unless setSHA256 was called)
    String getSHA256() const {
        if (!_expected_sha.length()) {
            return String();
        }
        return getDigestBase64(_sha);
    }

    // Amount of data written so far
    size_t getWrittenSize() const { return _written; }

    virtual void   apply() = 0;
    virtual bool   canRollback() = 0;
    virtual void   rollback() = 0;
    virtual bool   isRunning() const = 0;
    virtual String errorString() = 0;

protected:

    virtual bool doBegin(size_t size) = 0;
    virtual bool doWrite(const uint8_t* buff, unsigned len) = 0;
    virtual bool doEnd() = 0;
    virtual void doAbort() = 0;

private:

    void reset() {
        _expected_sha = "";
        _sha.reset();
        _written = 0;
    }

    DigestEngineSHA256  _sha;
    String              _expected_sha;
    size_t              _written = 0;
};

#if defined(ESP32)
  #include <updater/BlynkUpdaterESP32.h>
#elif defined(ESP8266)
  #include <updater/BlynkUpdaterESP8266.h>
#else
  #include <updater/BlynkUpdaterArduino.h>
#endif

inline BlynkUpdater& getBlynkUpdater() {
    // A single updater instance, shared by all translation units.
    static BlynkUpdater instance;
    return instance;
}

#define ArduinoUpdate  (getBlynkUpdater())

#endif
