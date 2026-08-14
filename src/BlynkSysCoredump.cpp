/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <BlynkSysUtils.h>

#if defined(ESP32)

extern "C" {
  #include "esp_core_dump.h"
}

size_t systemCoreDumpSize()
{
  size_t size = 0;
  size_t address = 0;
  if (esp_core_dump_image_get(&address, &size) == ESP_OK) {
    return size;
  }
  return 0;
}

void systemCoreDumpWrite(Stream& stream)
{
  size_t size = 0;
  size_t address = 0;
  if (esp_core_dump_image_get(&address, &size) == ESP_OK) {
    const esp_partition_t* pt = NULL;
    pt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");
    if (pt) {
      uint8_t bf[256];
      for (int16_t i = 0; i < (size / 256) + 1; i++) {
        int16_t toRead = (size - i * 256) > 256 ? 256 : (size - i * 256);
        esp_err_t er = esp_partition_read(pt, i * 256, bf, toRead);
        if (er == ESP_OK) {
          stream.write(bf, toRead);
        } else {
          stream.printf("FAIL [%x]\n",er);
        }
      }
    } else {
      stream.println(F("Partition NULL"));
    }
  } else {
    stream.println(F("No coredump found"));
  }
}

void systemCoreDumpClear()
{
  esp_core_dump_image_erase();
}

#else

size_t systemCoreDumpSize() {
  return 0;
}

void systemCoreDumpWrite(Stream& stream) {
  return;
}

void systemCoreDumpClear() {
}

#endif
