/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BlynkBleOTA_h
#define BlynkBleOTA_h

#include <mutex>

#include <ArduinoJson.h>
#include <digests/DigestEngines.h>
#include <updater/BlynkUpdater.h>

// For testing integrity checks, 1-100%
//#define BLE_OTA_TEST_CLOBBER_CHUNKS 1
//#define BLE_OTA_DUMP_INVALID_BLOCKS

//#define BLE_OTA_USE_CHUNK_HEADER

#define BLE_OTA_BUFFER_SIZE 4096

/*
 * Bare-metal toolchains (i.e. arm-none-eabi, used for SAMD) ship a libstdc++
 * built without threading support, so std::mutex is not available there.
 * On such platforms we fall back to a no-op lock: the transfer is lock-step
 * (the peer waits for ota_block_ok before sending the next block),
 * so the pending buffer is not accessed concurrently.
 */
#if defined(_GLIBCXX_HAS_GTHREADS) || defined(_LIBCPP_VERSION)
typedef std::mutex BlynkBleOtaLock;
#else
class BlynkBleOtaLock {
public:
    void lock()   {}
    void unlock() {}
};
#endif

/*
 * The firmware arrives in blocks of up to BLE_OTA_BUFFER_SIZE bytes,
 * each block split into MTU-sized chunks sent over the raw characteristic
 * (i.e. from the BLE stack context, hence the mutex).
 * A block is only handed over to the updater once the peer confirms it
 * with ota_block_apply, and the CRC32 of the collected chunks matches.
 * The updater verifies the SHA256 of the whole image before applying it.
 */

class BlynkBleOTA {
public:
    BlynkBleOTA(BlynkBLE& ble)
        : _ble(ble)
    {}

    void processCommand(JsonObject& json) {
        String t = json["t"];

        if (t == "ota_get_info") {
            const int chunk = _ble.getMTU()-3;
            char buff[256];
            JsonDocument doc;
            doc["t"] = "ota_info";
            doc["max_chunk"] = chunk;
            doc["max_block"] = BLE_OTA_BUFFER_SIZE / chunk;
            if (ArduinoUpdate.isRunning()) {
                doc["offset"] = ArduinoUpdate.getWrittenSize();
                doc["sha256"] = ArduinoUpdate.getSHA256();
            }
            size_t len = serializeJson(doc, buff, sizeof(buff));
            sendMsg(buff, len);
        } else if (t == "ota_start") {
            String   name   = json["name"].as<String>();
            String   sha    = json["sha256"].as<String>();
            unsigned offset = json["offset"].as<unsigned>();
            unsigned size   = json["size"].as<unsigned>();

            const char* error_msg = nullptr;
            if (!name.endsWith(".bin")) {
                error_msg = "wrong filename";
            } else if (size == 0) {
                error_msg = "wrong size";
            } else if (ArduinoUpdate.isRunning() &&
                       sha.length() && sha == _expected_sha)
            {
                // Try resuming
                if (offset != ArduinoUpdate.getWrittenSize()) {
                    error_msg = "offset not matching";
                } else if (size != _total_size) {
                    error_msg = "file size not matching";
                } else {
                    LOG_I("Resuming update");
                    std::lock_guard<BlynkBleOtaLock> lock(_mutex);
                    resetPending();
                }
            } else {
                abortUpdate();
                if (!ArduinoUpdate.begin(size)) {
                    LOG_E("Update: %s", ArduinoUpdate.errorString().c_str());
                    error_msg = "not enough space";
                } else if (!ArduinoUpdate.setSHA256(sha.c_str())) {
                    ArduinoUpdate.abort();
                    error_msg = "wrong sha256";
                } else {
                    _expected_sha = sha;
                    _total_size   = size;
                }
            }

            if (!error_msg) {
                sendMsg(R"json({"t":"ota_start_ok"})json");
            } else {
                LOG_E("OTA: %s", error_msg);
                char buff[256];
                JsonDocument doc;
                doc["t"] = "ota_start_fail";
                doc["msg"] = error_msg;
                size_t len = serializeJson(doc, buff, sizeof(buff));
                sendMsg(buff, len);
            }

        } else if (t == "ota_block_apply") {
            unsigned offset = json["offset"].as<unsigned>();
            String exp_crc  = json["crc"].as<String>();

            exp_crc.toLowerCase();

            std::unique_lock<BlynkBleOtaLock> lock(_mutex);

            String act_crc  = getDigestHex(_pending_crc);

            /*LOG_I("Block %d crc: %s (need:%d, size:%d)",
                  offset, act_crc.c_str(),
                  _pending_size,
                  ArduinoUpdate.getWrittenSize());*/

            String error_msg = "none";
            bool error_fatal = false;
            if (!ArduinoUpdate.isRunning()) {
                error_fatal = true;
                error_msg  = ArduinoUpdate.errorString();
            } else if (offset != ArduinoUpdate.getWrittenSize()) {
                error_fatal = true;
                error_msg  = "block offset invalid";
            } else if (exp_crc != act_crc) {
                LOG_E("CRC mismatch (expected: %s, actual: %s)", exp_crc.c_str(), act_crc.c_str());
#ifdef BLE_OTA_DUMP_INVALID_BLOCKS
                for (int i = 0; i < _pending_size; i++) {
                    Serial.printf("%02x", _pending_bytes[i]);
                    if ((i % 32) == 31) { Serial.println(); }
                }
                Serial.println();
#endif
                resetPending();
                error_msg = "block crc invalid";
            } else if (ArduinoUpdate.getWrittenSize() + _pending_size > _total_size) {
                error_fatal = true;
                error_msg  = "file size exceeded";
            } else if (!ArduinoUpdate.write(_pending_bytes, _pending_size)) {
                error_fatal = true;
                error_msg  = ArduinoUpdate.errorString();
            } else {
                resetPending();
            }

            if (error_fatal) {
                abortUpdateLocked();
            }
            lock.unlock();

            if (error_msg == "none") {
                sendMsg(R"json({"t":"ota_block_ok"})json");
            } else {
                if (error_fatal) {
                    LOG_E("OTA: %s", error_msg.c_str());
                } else {
                    LOG_W("%s", error_msg.c_str());
                }
                char buff[256];
                JsonDocument doc;
                doc["t"] = error_fatal ? "ota_fail" : "ota_block_fail";
                doc["msg"] = error_msg;
                size_t len = serializeJson(doc, buff, sizeof(buff));
                sendMsg(buff, len);
            }

        } else if (t == "ota_finish") {
            LOG_I("OTA: %s size: %d",
                  ArduinoUpdate.getSHA256().c_str(),
                  ArduinoUpdate.getWrittenSize()
                );

            const char* error_msg = nullptr;
            if (!ArduinoUpdate.isRunning()) {
                error_msg = "update not running";
            } else if (ArduinoUpdate.getWrittenSize() != _total_size) {
                error_msg = "file incomplete";
            } else if (!ArduinoUpdate.end()) {
                // SHA256 (or the platform's own checks) didn't pass
                error_msg = "verification failed";
            }

            if (!error_msg) {
                sendMsg(R"json({"t":"ota_ok"})json");
                ArduinoUpdate.apply();
            } else {
                LOG_E("OTA: %s", error_msg);
                char buff[256];
                JsonDocument doc;
                doc["t"] = "ota_fail";
                doc["msg"] = error_msg;
                size_t len = serializeJson(doc, buff, sizeof(buff));
                sendMsg(buff, len);
                abortUpdate();
            }
        } else if (t == "ota_cancel") {
            abortUpdate();
            sendMsg(R"json({"t":"ota_cancel_ok"})json");
        } else if (t == "ota_rollback") {
            if (ArduinoUpdate.canRollback()) {
                sendMsg(R"json({"t":"ota_rollback_ok"})json");
                ArduinoUpdate.rollback();
            } else {
                sendMsg(R"json({"t":"ota_rollback_fail"})json");
            }
        }
    }

    void processRawPacket(const uint8_t* data, size_t len) {
        std::lock_guard<BlynkBleOtaLock> lock(_mutex);
#ifdef BLE_OTA_USE_CHUNK_HEADER
        unsigned chunk_id = data[0];
        data++; len--;
        if (chunk_id != _pending_chunk) {
            LOG_E("OTA: chunk out of order (exp: %d, got: %d)", _pending_chunk, chunk_id);
        }
        _pending_chunk++;
#endif
        if (_pending_size + len <= sizeof(_pending_bytes)) {
#ifdef BLE_OTA_TEST_CLOBBER_CHUNKS
            if (random(0, 100) < BLE_OTA_TEST_CLOBBER_CHUNKS) {
                ((uint8_t*)data)[0] = 0xFA;
            }
#endif
            memcpy(_pending_bytes+_pending_size, data, len);
            _pending_size += len;
            _pending_crc.update(data, len);
        } else {
            LOG_E("OTA pending buffer overflow");
        }
    }

private:
    void sendMsg(const char* str) {
        _ble.write(str, strlen(str));
    }
    void sendMsg(const void* data, unsigned len) {
        _ble.write(data, len);
    }

    // Needs _mutex held
    void resetPending() {
#ifdef BLE_OTA_USE_CHUNK_HEADER
        _pending_chunk = 0;
#endif
        _pending_size = 0;
        _pending_crc.reset();
        memset(_pending_bytes, 0, sizeof(_pending_bytes));
    }

    // Needs _mutex held
    void abortUpdateLocked() {
        resetPending();
        _expected_sha = "";
        _total_size = 0;
        if (ArduinoUpdate.isRunning()) {
            ArduinoUpdate.abort();
        }
    }

    void abortUpdate() {
        std::lock_guard<BlynkBleOtaLock> lock(_mutex);
        abortUpdateLocked();
    }

private:
    BlynkBLE&                     _ble;

    String                        _expected_sha;
    unsigned                      _total_size = 0;

    BlynkBleOtaLock               _mutex;
    uint8_t                       _pending_bytes[BLE_OTA_BUFFER_SIZE];
    unsigned                      _pending_size = 0;
    DigestEngineCRC32             _pending_crc;
#ifdef BLE_OTA_USE_CHUNK_HEADER
    unsigned                      _pending_chunk = 0;
#endif
};

#endif
