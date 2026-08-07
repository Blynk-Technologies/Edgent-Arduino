/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Single-color LED driver using PWM.
 * All configuration via constructor parameters — no board macros required.
 */

#pragma once

#include <Arduino.h>

extern const unsigned char _gamma8[256];

class LedMono {
public:
  static constexpr bool isRGB = false;

  LedMono(uint8_t pin, bool inverse = false, uint16_t pwmRange = 256)
    : _pin(pin), _inverse(inverse), _pwmRange(pwmRange)
  {}

  void initLED() {
    pinMode(_pin, OUTPUT);
  }

  void setRGB(uint32_t color) {
    // Extract perceived brightness from RGB (max-channel approximation)
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >>  8) & 0xFF;
    uint8_t b = (color >>  0) & 0xFF;
    uint8_t brightness = r > g ? (r > b ? r : b) : (g > b ? g : b);
    setLED(brightness);
  }

  void setLED(uint32_t brightness) {
    uint8_t v = brightness > 255 ? 255 : (uint8_t)brightness;
    if (_inverse) {
      v = 255 - v;
    }
    analogWrite(_pin, toPwm(v));
  }

private:
  uint32_t toPwm(uint8_t v) const {
    return (uint32_t)(_gamma8[v]) * (_pwmRange / 256);
  }

  uint8_t  _pin;
  bool     _inverse;
  uint16_t _pwmRange;
};