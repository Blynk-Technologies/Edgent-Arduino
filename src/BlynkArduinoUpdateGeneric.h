/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BlynkArduinoUpdateGeneric_h
#define BlynkArduinoUpdateGeneric_h

// From ArduinoOTA.h
#ifdef __AVR__
#if FLASHEND >= 0xFFFF
#include "InternalStorageAVR.h"
#endif
#elif defined(ARDUINO_ARCH_STM32)
#include <InternalStorageSTM32.h>
#elif defined(ARDUINO_ARCH_RP2040)
#include <InternalStorageRP2.h>
#elif defined(ESP8266) || defined(ESP32)
#include "InternalStorageESP.h"
#elif defined(ARDUINO_AMEBA)
  #warning "Blynk.Air: OTA update not implemented for this Primary MCU"

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

class BlynkArduinoUpdater {
public:

    bool begin() {
        if (_is_running) {
            return false;
        }
        //InternalStorage.debugPrint();
        _is_running = InternalStorage.open(InternalStorage.maxSize());
        return _is_running;
    }

    bool setMD5(const char* expected_md5) {
        // TODO: check MD5 of the written data, if set
        (void)expected_md5;
        return true;
    }

    bool write(const uint8_t* buff, unsigned len) {
        for (unsigned i = 0; i < len; i++) {
            InternalStorage.write(buff[i]);
        }
        return true;
    }

    bool end() {
        InternalStorage.close();
        _is_running = false;
        return true;
    }

    void apply() {
        delay(50);
        InternalStorage.apply();
    }

    void abort() {
        InternalStorage.close();
        _is_running = false;
    }

    bool canRollback()      { return false; }
    void rollback()         { /* not supported */ }

    bool isRunning() const  { return _is_running; }
    String errorString()    { return "internal error"; }

private:
    bool _is_running;
};

#endif
