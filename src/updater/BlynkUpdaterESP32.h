/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BlynkUpdaterESP32_h
#define BlynkUpdaterESP32_h

#include <Update.h>

class BlynkUpdater
    : public BlynkUpdaterBase
{
public:

    void apply() override {
        delay(50);
        ESP.restart();
    }

    bool canRollback() override {
        return Update.canRollBack();
    }

    void rollback() override {
        if (canRollback()) {
            Update.rollBack();
            apply();
        }
    }

    bool isRunning() const override  { return Update.isRunning(); }
    String errorString() override    { return Update.errorString(); }

protected:

    bool doBegin(size_t size) override {
        return Update.begin(size ? size : UPDATE_SIZE_UNKNOWN);
    }

    bool doWrite(const uint8_t* buff, unsigned len) override {
        return (len == Update.write((uint8_t*)buff, len));
    }

    bool doEnd() override {
        return Update.end(true);
    }

    void doAbort() override {
        Update.abort();
    }
};

#endif
