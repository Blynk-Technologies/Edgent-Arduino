/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "EdgentSettings.h"

#if defined(BLYNK_USE_LITTLEFS)
  #include <LittleFS.h>
  #define BLYNK_FS LittleFS
#elif defined(BLYNK_USE_SPIFFS) && defined(ESP32)
  #include <SPIFFS.h>
  #define BLYNK_FS SPIFFS
#elif defined(BLYNK_USE_SPIFFS) && defined(ESP8266)
  #include <FS.h>
  #define BLYNK_FS SPIFFS
  #define FILE_READ  "r"
  #define FILE_WRITE "w"
#endif

#if defined(BLYNK_NOINIT_ATTR)
  // OK, use it
#elif defined(ESP32)
  #define BLYNK_NOINIT_ATTR     __NOINIT_ATTR
  //#define BLYNK_NOINIT_ATTR   RTC_NOINIT_ATTR
#elif defined(PARTICLE)
  #define BLYNK_NOINIT_ATTR     // Not supported
#else
  #define BLYNK_NOINIT_ATTR     __attribute__((section(".noinit")))
#endif

String    timeSpanToStr(const uint64_t t);

void      systemInit(String devPrefix, String devName);
String    systemGetDeviceName(bool withPrefix = true);
String    systemGetDeviceUID();
uint64_t  systemUptime();
void      systemReboot();
String    systemGetResetReason();
String    systemGetFlashMode();
size_t    systemCoreDumpSize();
void      systemCoreDumpWrite(Stream& stream);
void      systemCoreDumpClear();

class SystemStats {
public:
  struct {
    uint32_t total;
    uint32_t graceful;
  } resetCount;

  uint32_t cloud_drops;
  uint32_t network_drops;
  uint32_t max_online_time;
  uint32_t max_offline_time;
  uint32_t total_online_time;
  uint32_t total_offline_time;

public:
  SystemStats() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    if (_magic != expectedMagic()) {
      clear();
    } else {
      resetCount.total++;
    }
#pragma GCC diagnostic pop
  }

  void clear() {
    memset(this, 0, sizeof(SystemStats));
    _magic = expectedMagic();
  }

public:
  void trackConnected() {
    const uint64_t now = systemUptime();
    const uint32_t delta_secs = (now - _last_connected_change) / 1000;
    total_offline_time += delta_secs;
    max_offline_time = max(max_offline_time, delta_secs);
    _last_connected_change = now;
  }

  void trackDisconnected() {
    const uint64_t now = systemUptime();
    const uint32_t delta_secs = (now - _last_connected_change) / 1000;
    total_online_time += delta_secs;
    max_online_time = max(max_online_time, delta_secs);
    _last_connected_change = now;
  }

private:
  uint64_t _last_connected_change;

private:
  static uint32_t expectedMagic() {
    return (MAGIC + __LINE__ + sizeof(SystemStats));
  }
  static const uint32_t MAGIC = 0x2f5385a4;
  uint32_t _magic;
};

extern SystemStats systemStats;

