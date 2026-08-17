/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BlynkUpdaterArduino_h
#define BlynkUpdaterArduino_h

// From ArduinoOTA.h
#ifdef __AVR__
#if FLASHEND >= 0xFFFF
#include "InternalStorageAVR.h"
#endif
#elif defined(ARDUINO_ARCH_STM32)
#include <InternalStorageSTM32.h>
#elif defined(ARDUINO_ARCH_RP2040)
#include <InternalStorageRP2.h>
#elif defined(ARDUINO_ARCH_RENESAS_UNO)
#include <InternalStorageRenesas.h>
#elif defined(ESP8266) || defined(ESP32)
#include "InternalStorageESP.h"
#elif defined(ARDUINO_AMEBA)
  #warning "Blynk.Air: OTA update not implemented for Realtek Ameba"

  class InternalStorageClass {
  public:
    virtual int open(int) { return false; }
    virtual size_t write(uint8_t) { return 0; }
    virtual void close() {}
    virtual void clear() {}
    virtual void apply() {}
    virtual long maxSize() { return 0; }
  };
  static InternalStorageClass InternalStorage;
#else
#include "InternalStorage.h"
#endif
// End ArduinoOTA.h

class BlynkUpdater
    : public BlynkUpdaterBase
{
public:

    void apply() override {
        delay(50);
        InternalStorage.apply();
    }

    bool canRollback() override      { return false; }
    void rollback() override         { /* not supported */ }

    bool isRunning() const override  { return _is_running; }
    String errorString() override    { return "internal error"; }

protected:

    bool doBegin(size_t size) override {
        if (_is_running) {
            return false;
        }
        const long maxSize = InternalStorage.maxSize();
        if (maxSize <= 0) {
            return false;
        }
        if (!size) {
            size = size_t(maxSize);
        } else if (long(size) > maxSize) {
            return false;
        }
        //InternalStorage.debugPrint();
        _is_running = InternalStorage.open(int(size));
        return _is_running;
    }

    bool doWrite(const uint8_t* buff, unsigned len) override {
        for (unsigned i = 0; i < len; i++) {
            if (1 != InternalStorage.write(buff[i])) {
                return false;
            }
        }
        return true;
    }

    bool doEnd() override {
        InternalStorage.close();
        _is_running = false;
        return true;
    }

    void doAbort() override {
        InternalStorage.close();
        _is_running = false;
    }

private:
    bool _is_running = false;
};

#endif
