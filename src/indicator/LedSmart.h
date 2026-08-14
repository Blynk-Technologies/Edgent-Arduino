/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Addressable RGB LED driver (NeoPixel / WS2812 / SK6812).
 * All configuration via constructor parameters — no board macros required.
 */

#pragma once

#include <Adafruit_NeoPixel.h>

class LedSmart {
public:
  static constexpr bool isRGB = true;

  LedSmart(uint8_t pin, neoPixelType type = NEO_GRB + NEO_KHZ800)
    : _pixel(1, pin, type)
  {}

  void initLED() {
    _pixel.begin();
  }

  void setRGB(uint32_t color) {
    _pixel.setPixelColor(0, color);
    _pixel.show();
  }

  void setLED(uint32_t brightness) {
    setRGB((brightness << 16) | (brightness << 8) | brightness);
  }

private:
  Adafruit_NeoPixel _pixel;
};
