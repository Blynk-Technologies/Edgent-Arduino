/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BlynkArduinoUpdateESP8266_h
#define BlynkArduinoUpdateESP8266_h

class BlynkArduinoUpdater {
public:

    bool begin() {
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        return Update.begin(maxSketchSpace);
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
        //TODO: Update.abort();
    }

    bool canRollback() {
        return false;
    }

    void rollback() {
        LOG_E("Rollback not supported on this platform");
    }

    bool isRunning() const  { return Update.isRunning(); }
    String errorString()    { return Update.getErrorString(); }
};

#endif
