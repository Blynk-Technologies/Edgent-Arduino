/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * RGB LED driver using PWM (common anode or common cathode).
 * All configuration via constructor parameters — no board macros required.
 */

#pragma once

#include <Arduino.h>
#include "GammaTable.h"

class LedRgbPwm {
public:
    static constexpr bool isRGB = true;

    LedRgbPwm(uint8_t pinR, uint8_t pinG, uint8_t pinB, bool inverse = false, uint16_t pwmRange = 256)
        : _pinR(pinR), _pinG(pinG), _pinB(pinB), _inverse(inverse), _pwmRange(pwmRange) {}

    void initLED() {
        pinMode(_pinR, OUTPUT);
        pinMode(_pinG, OUTPUT);
        pinMode(_pinB, OUTPUT);
    }

    void setRGB(uint32_t color) {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = (color >> 0) & 0xFF;

        if (_inverse) {
            r = 255 - r;
            g = 255 - g;
            b = 255 - b;
        }

        analogWrite(_pinR, toPwm(r));
        analogWrite(_pinG, toPwm(g));
        analogWrite(_pinB, toPwm(b));
    }

    void setLED(uint32_t brightness) {
        setRGB((brightness << 16) | (brightness << 8) | brightness);
    }

private:
    uint32_t toPwm(uint8_t v) const {
        return (uint32_t)(blnk_gamma8[v]) * (_pwmRange / 256);
    }

    uint8_t _pinR, _pinG, _pinB;
    bool _inverse;
    uint16_t _pwmRange;
};
