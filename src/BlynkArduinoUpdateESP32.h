/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BlynkArduinoUpdateESP32_h
#define BlynkArduinoUpdateESP32_h

#include <Update.h>

class BlynkArduinoUpdater {
public:

    bool begin() {
        return Update.begin(UPDATE_SIZE_UNKNOWN);
    }

    bool setMD5(const char* expected_md5) {
        return Update.setMD5(expected_md5);
    }

    bool write(const uint8_t* buff, unsigned len) {
        return (len == Update.write((uint8_t*)buff, len));
    }

    bool end() {
        return Update.end(true);
    }

    void apply() {
        delay(50);
        ESP.restart();
    }

    void abort() {
        Update.abort();
    }

    bool canRollback() {
        return Update.canRollBack();
    }

    void rollback() {
        if (canRollback()) {
            Update.rollBack();
            apply();
        }
    }

    bool isRunning() const  { return Update.isRunning(); }
    String errorString()    { return Update.errorString(); }
};

#endif
