/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BlynkUpdaterESP8266_h
#define BlynkUpdaterESP8266_h

class BlynkUpdater
    : public BlynkUpdaterBase
{
public:

    void apply() override {
        delay(50);
        ESP.restart();
    }

    bool canRollback() override {
        return false;
    }

    void rollback() override {
        LOG_E("Rollback not supported on this platform");
    }

    bool isRunning() const override  { return Update.isRunning(); }
    String errorString() override    { return Update.getErrorString(); }

protected:

    bool doBegin(size_t size) override {
        if (!size) {
            size = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        }
        return Update.begin(size);
    }

    bool doWrite(const uint8_t* buff, unsigned len) override {
        return (len == Update.write((uint8_t*)buff, len));
    }

    bool doEnd() override {
        return Update.end(true);
    }

    void doAbort() override {
        if (!Update.isRunning()) {
            return;
        }
        /*
         * The ESP8266 Updater has no abort(): end() on a fully received image
         * would commit it. Setting a MD5 that cannot match makes the final
         * check fail, so the image is dropped instead
         */
        Update.setMD5("00000000000000000000000000000000");
        Update.end();
        if (Update.isRunning()) {
            // An MD5 error does not release the updater by itself
            Update.end();
        }
        // Otherwise a new update cannot be started without a reboot
        Update.clearError();
    }
};

#endif
