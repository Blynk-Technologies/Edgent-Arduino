/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BlynkArduinoUpdate_h
#define BlynkArduinoUpdate_h

#if defined(ESP32)
  #include <BlynkArduinoUpdateESP32.h>
#elif defined(ESP8266)
  #include <BlynkArduinoUpdateESP8266.h>
#else
  #include <BlynkArduinoUpdateGeneric.h>
#endif

static BlynkArduinoUpdater ArduinoUpdate;

#endif
