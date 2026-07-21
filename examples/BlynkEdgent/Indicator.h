/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "BoardConfig.h"

#if defined(BOARD_LED_PIN_WS2812)
  #include <Adafruit_NeoPixel.h>    // Library: https://github.com/adafruit/Adafruit_NeoPixel
#endif

#if defined(USE_TICKER)
  #include <Ticker.h>
#elif defined(USE_PTHREAD)
  #include <pthread.h>
#elif defined(USE_TIMER_FIVE)
  #include <Timer5.h>
#endif

#if !defined(BOARD_LED_BRIGHTNESS)
#define BOARD_LED_BRIGHTNESS 255
#endif

#if !defined(BOARD_LED_MIN_BRIGHTNESS)
#define BOARD_LED_MIN_BRIGHTNESS 4
#endif

#if defined(BOARD_LED_PIN_WS2812) || defined(BOARD_LED_PIN_R)
#define BOARD_LED_IS_RGB
#endif

const uint8_t gamma8[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

#define DIMM(x)    (((uint32_t)(x)*(BOARD_LED_BRIGHTNESS))/255)
#define RGB(r,g,b) (DIMM(r) << 16 | DIMM(g) << 8 | DIMM(b) << 0)
#define TO_PWM(x)  ((uint32_t)(gamma8[(x)])*(BOARD_PWM_RANGE/256))

class Indicator {
public:

  enum Mode {
    STAY_OFF,
    BLINK_BLUE_SLOW,
    BLINK_BLUE_FAST,
    BLINK_GREEN_SLOW,
    BLINK_GREEN_FAST,
    WAVE_GREEN_SLOW,
    BLINK_WHITE_FAST,
    BLINK_WHITE_XFAST,
    WAVE_WHITE_FAST,
    BLINK_MAGENTA_FAST,
    HEARTBEAT_RED
  };

  enum Colors {
    COLOR_BLACK   = RGB(0x00, 0x00, 0x00),
    COLOR_WHITE   = RGB(0xFF, 0xFF, 0xE7),

    COLOR_RED     = RGB(0xFF, 0x00, 0x00),
    COLOR_GREEN   = RGB(0x00, 0xFF, 0x00),
    COLOR_BLUE    = RGB(0x00, 0x00, 0xFF),

    COLOR_BLYNK   = RGB(0x24, 0xC4, 0x8E),
    COLOR_MAGENTA = RGB(0xA7, 0x00, 0xFF),
  };

  Indicator() {
    _counter = 0;
  }

  void begin() {
    initLED();
    off();
    initTimer();
  }

  void setMode(Mode m) {
    _mode = m;
  }

protected:

  uint32_t run() {
    // Reset counter if indicator state changes
    if (_modePrev != _mode) {
      _modePrev = _mode;
      _counter = 0;
      if (_mode == STAY_OFF) {
        off();
      }
    }

    switch (_mode) {
    case STAY_OFF:              return skipLED();
    case BLINK_BLUE_SLOW:       return beatLED(COLOR_BLUE,    (int[]){ 50, 500 });
    case BLINK_BLUE_FAST:       return beatLED(COLOR_BLUE,    (int[]){ 200, 200 });
    case BLINK_GREEN_SLOW:      return beatLED(COLOR_GREEN,   (int[]){ 50, 500 });
    case BLINK_GREEN_FAST:      return beatLED(COLOR_GREEN,   (int[]){ 100, 100 });
    case WAVE_GREEN_SLOW:       return waveLED(COLOR_BLYNK,   5000);
    case BLINK_WHITE_FAST:      return beatLED(COLOR_WHITE,   (int[]){ 50, 200 });
    case BLINK_WHITE_XFAST:     return beatLED(COLOR_WHITE,   (int[]){ 50, 50 });
    case WAVE_WHITE_FAST:       return waveLED(COLOR_WHITE,   500);
    case BLINK_MAGENTA_FAST:    return beatLED(COLOR_MAGENTA, (int[]){ 50, 50 });
    case HEARTBEAT_RED:         return beatLED(COLOR_RED,     (int[]){ 80, 100, 80, 1000 } );
    default:                    return skipLED();
    }
  }

  void off() {
#if defined(BOARD_LED_IS_RGB)
    setRGB(COLOR_BLACK);
#else
    setLED(0);
#endif
    _mode = STAY_OFF;
  }

  /*
   * Animation timer
   */

#if defined(USE_TICKER)
  Ticker _blinker;

  static
  void timerCbk(Indicator* instance) {
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

  static
  void* timerThread(void* instance) {
    while (true) {
      uint32_t returnTime = ((Indicator*)instance)->run();
      returnTime = BlynkMathClamp(returnTime, 1, 10000);
      vTaskDelay(returnTime);
    }
  }

  void initTimer() {
    pthread_create(&_blinker, NULL, timerThread, this);
  }

#elif defined(USE_TIMER_FIVE)
  static Indicator* _instance;
  int _timer_counter;

  static
  void timerCbk() {
    _instance->_timer_counter -= 10;
    if (_instance->_timer_counter < 0) {
      _instance->_timer_counter = _instance->run();
    }
  }

  void initTimer() {
    _instance = this;
    _timer_counter = -1;

    MyTimer5.begin(1000/10);
    MyTimer5.attachInterrupt(timerCbk);
    MyTimer5.start();
  }


#else

  #warning "LED indicator needs a functional timer"

  void initTimer() {}

#endif



  /*
   * LED drivers
   */

#if defined(BOARD_LED_PIN_WS2812)  // Addressable, NeoPixel RGB LED

  Adafruit_NeoPixel _pixel = Adafruit_NeoPixel(1, BOARD_LED_PIN_WS2812, NEO_GRB + NEO_KHZ800);

  void initLED() {
    _pixel.begin();
  }

  void setRGB(uint32_t color) {
    _pixel.setPixelColor(0, color);
    _pixel.show();
  }

#elif defined(BOARD_LED_PIN_R)     // Normal RGB LED (common anode or common cathode)

  void initLED() {
    pinMode(BOARD_LED_PIN_R, OUTPUT);
    pinMode(BOARD_LED_PIN_G, OUTPUT);
    pinMode(BOARD_LED_PIN_B, OUTPUT);
  }

  void setRGB(uint32_t color) {
    uint8_t r = (color & 0xFF0000) >> 16;
    uint8_t g = (color & 0x00FF00) >> 8;
    uint8_t b = (color & 0x0000FF);

    #if BOARD_LED_INVERSE
    analogWrite(BOARD_LED_PIN_R, TO_PWM(255 - r));
    analogWrite(BOARD_LED_PIN_G, TO_PWM(255 - g));
    analogWrite(BOARD_LED_PIN_B, TO_PWM(255 - b));
    #else
    analogWrite(BOARD_LED_PIN_R, TO_PWM(r));
    analogWrite(BOARD_LED_PIN_G, TO_PWM(g));
    analogWrite(BOARD_LED_PIN_B, TO_PWM(b));
    #endif
  }

#elif defined(BOARD_LED_PIN)       // Single color LED

  void initLED() {
    pinMode(BOARD_LED_PIN, OUTPUT);
  }

  void setLED(uint32_t color) {
    #if BOARD_LED_INVERSE
    analogWrite(BOARD_LED_PIN, TO_PWM(255 - color));
    #else
    analogWrite(BOARD_LED_PIN, TO_PWM(color));
    #endif
  }

#else

  #warning "Invalid LED configuration"

  void initLED() {
  }

  void setLED(uint32_t color) {
  }

#endif

  /*
   * Animations
   */

  uint32_t skipLED() {
    return 100;
  }

#if defined(BOARD_LED_IS_RGB)

  template<typename T>
  uint32_t beatLED(uint32_t onColor, const T& beat) {
    const uint8_t cnt = sizeof(beat)/sizeof(beat[0]);
    setRGB((_counter % 2 == 0) ? onColor : (uint32_t)COLOR_BLACK);
    uint32_t next = beat[_counter % cnt];
    _counter = (_counter+1) % cnt;
    return next;
  }

  uint32_t waveLED(uint32_t colorMax, unsigned breathePeriod) {
    uint8_t redMax   = (colorMax & 0xFF0000) >> 16;
    uint8_t greenMax = (colorMax & 0x00FF00) >> 8;
    uint8_t blueMax  = (colorMax & 0x0000FF);

    // Brightness will rise from 0 to 128, then fall back to 0
    float brightness = (_counter < 128) ? _counter : 255 - _counter;
    brightness = BlynkMathMap(brightness, 0, 127, BOARD_LED_MIN_BRIGHTNESS, 127) / 128.0f;

    // Multiply our three colors by the brightness:
    redMax   *= brightness;
    greenMax *= brightness;
    blueMax  *= brightness;
    // And turn the LED to that color:
    setRGB((redMax << 16) | (greenMax << 8) | blueMax);

    // This function relies on the 8-bit, unsigned _counter rolling over.
    _counter = (_counter+1) % 256;
    return breathePeriod / 256;
  }

#else

  template<typename T>
  uint32_t beatLED(uint32_t, const T& beat) {
    const uint8_t cnt = sizeof(beat)/sizeof(beat[0]);
    setLED((_counter % 2 == 0) ? BOARD_LED_BRIGHTNESS : 0);
    uint32_t next = beat[_counter % cnt];
    _counter = (_counter+1) % cnt;
    return next;
  }

  uint32_t waveLED(uint32_t, unsigned breathePeriod) {
    uint32_t brightness = (_counter < 128) ? _counter : 255 - _counter;
    brightness = BlynkMathMap(brightness, 0, 127, BOARD_LED_MIN_BRIGHTNESS, 127);

    setLED(DIMM(brightness*2));

    // This function relies on the 8-bit, unsigned _counter rolling over.
    _counter = (_counter+1) % 256;
    return breathePeriod / 256;
  }

#endif

private:
  uint8_t _counter;
  Mode    _mode;
  Mode    _modePrev;
};

