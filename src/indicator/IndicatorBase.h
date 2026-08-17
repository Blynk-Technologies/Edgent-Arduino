/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * IndicatorBase<LedDriver> - LED indicator with animations.
 *
 * Template parameter LedDriver must provide:
 *   static constexpr bool isRGB;
 *   void initLED();
 *   void setRGB(uint32_t color);      // packed 0xRRGGBB
 *   void setLED(uint32_t brightness); // 0..255
 *
 * Timer backends selected via USE_TICKER / USE_PTHREAD / USE_TIMER_FIVE macros.
 */

#pragma once

#include <stdint.h>

#if defined(USE_TICKER) || defined(USE_PTHREAD) || defined(USE_TIMER_FIVE)
  // OK, use it
#elif defined(ESP32)
  #define USE_PTHREAD
#elif defined(ESP8266)
  #define USE_TICKER
#endif

#if defined(USE_TICKER)
  #include <Ticker.h>
#elif defined(USE_PTHREAD)
  #include <pthread.h>
#elif defined(USE_TIMER_FIVE)
  #include <Timer5.h>
#endif


template <class LedDriver>
class IndicatorBase : public LedDriver {
public:
  // Forward all args to LedDriver constructor
    using LedDriver::LedDriver;

    enum Colors : uint32_t {
        COLOR_BLACK = 0x000000,
        COLOR_WHITE = 0xFFFFE7,

        COLOR_RED = 0xFF0000,
        COLOR_GREEN = 0x00FF00,
        COLOR_BLUE = 0x0000FF,

        COLOR_CYAN = 0x00FFFF,
        COLOR_MAGENTA = 0xA700FF,
        COLOR_YELLOW = 0xFFFF00,

        COLOR_ORANGE = 0xFF7F00,
        COLOR_PINK = 0xFF00FF,
        COLOR_PURPLE = 0x7F00FF,
        COLOR_ROSE = 0xFF007F,
        COLOR_LIME = 0x7FFF00,
        COLOR_TEAL = 0x00FF7F,
        COLOR_INDIGO = 0x4B0082,

        COLOR_BLYNK = 0x24C48E,
    };

    void begin() {
        this->initLED();
        off();
        initTimer();
    }

    void setMode(int m) {
        _mode = m;
    }

    int getMode() const {
        return _mode;
    }

    void setBrightness(uint8_t brightness, uint8_t minBrightness = 4) {
        _brightness = brightness;
        _minBrightness = minBrightness;
    }

protected:
  /*
   * Override this in a subclass to define your own mode-to-animation mapping.
   * Return value is the delay in ms before the next call.
   */
    virtual uint32_t run() {
        if (_modePrev != _mode) {
            _modePrev = _mode;
            _counter = 0;
            if (_mode == 0) {
                off();
            }
        }
        return skipLED();
    }

    void off() {
        if constexpr (LedDriver::isRGB) {
            this->setRGB(COLOR_BLACK);
        } else {
            this->setLED(0);
        }
    }

  // Apply brightness scaling to a color
    uint32_t dimm(uint32_t color) const {
        uint8_t r = ((color >> 16) & 0xFF) * _brightness / 255;
        uint8_t g = ((color >> 8) & 0xFF) * _brightness / 255;
        uint8_t b = ((color >> 0) & 0xFF) * _brightness / 255;
        return (r << 16) | (g << 8) | b;
    }

  /*
   * Animations
   */

    uint32_t skipLED() {
        return 100;
    }

    template <typename T>
    uint32_t beatLED(uint32_t onColor, const T& beat) {
        const uint8_t cnt = sizeof(beat) / sizeof(beat[0]);
        if constexpr (LedDriver::isRGB) {
            this->setRGB((_counter % 2 == 0) ? dimm(onColor) : (uint32_t)COLOR_BLACK);
        } else {
            this->setLED((_counter % 2 == 0) ? _brightness : 0);
        }
        uint32_t next = beat[_counter % cnt];
        _counter = (_counter + 1) % cnt;
        return next;
    }

    uint32_t waveLED(uint32_t colorMax, unsigned breathePeriod) {
        if constexpr (LedDriver::isRGB) {
            uint32_t c = dimm(colorMax);
            uint8_t redMax = (c >> 16) & 0xFF;
            uint8_t greenMax = (c >> 8) & 0xFF;
            uint8_t blueMax = (c >> 0) & 0xFF;

            float brightness = (_counter < 128) ? _counter : 255 - _counter;
            brightness = BlynkMathMap(brightness, 0, 127, (int)_minBrightness, 127) / 128.0f;

            redMax *= brightness;
            greenMax *= brightness;
            blueMax *= brightness;
            this->setRGB((redMax << 16) | (greenMax << 8) | blueMax);
        } else {
            uint32_t brightness = (_counter < 128) ? _counter : 255 - _counter;
            brightness = BlynkMathMap(brightness, 0, 127, (int)_minBrightness, 127);
            this->setLED((uint32_t)brightness * _brightness * 2 / 255);
        }
        _counter = (_counter + 1) % 256;
        return breathePeriod / 256;
    }

    uint8_t _counter = 0;
    int _mode = 0;
    int _modePrev = -1;

private:
    uint8_t _brightness = 255;
    uint8_t _minBrightness = 4;

  /*
   * Animation timer
   */

#if defined(USE_TICKER)
    Ticker _blinker;

    static void timerCbk(IndicatorBase* instance) {
        uint32_t returnTime = instance->run();
        if (returnTime) {
            instance->_blinker.attach_ms(returnTime, timerCbk, instance);
        }
    }

    void initTimer() {
        _blinker.attach_ms(100, timerCbk, this);
    }

#elif defined(USE_PTHREAD)
    pthread_t _blinker;

    static void* timerThread(void* instance) {
        while (true) {
            uint32_t returnTime = ((IndicatorBase*)instance)->run();
            returnTime = BlynkMathClamp(returnTime, 1, 10000);
            vTaskDelay(returnTime);
        }
    }

    void initTimer() {
        pthread_create(&_blinker, NULL, timerThread, this);
    }

#elif defined(USE_TIMER_FIVE)
    static IndicatorBase* _instance;
    int _timer_counter;

    static void timerCbk() {
        _instance->_timer_counter -= 10;
        if (_instance->_timer_counter < 0) {
            _instance->_timer_counter = _instance->run();
        }
    }

    void initTimer() {
        _instance = this;
        _timer_counter = -1;
        MyTimer5.begin(1000 / 10);
        MyTimer5.attachInterrupt(timerCbk);
        MyTimer5.start();
    }

#else
  #warning "LED indicator needs a functional timer"
    void initTimer() {}
#endif
};

#if defined(USE_TIMER_FIVE)
template <class LedDriver>
IndicatorBase<LedDriver>* IndicatorBase<LedDriver>::_instance = nullptr;
#endif
