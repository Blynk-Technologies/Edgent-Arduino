/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "BoardConfig.h"
#include "Indicator.h"

Indicator userLED;

void updateIndicator()
{
  switch (BlynkEdgent.getState()) {
  case Edgent::MODE_WAIT_CONFIG:      userLED.setMode(Indicator::BLINK_BLUE_SLOW);    break;
  case Edgent::MODE_CONNECTING_NET:
  case Edgent::MODE_CONNECTING_CLOUD: userLED.setMode(Indicator::BLINK_GREEN_SLOW);   break;
  case Edgent::MODE_RUNNING:          userLED.setMode(Indicator::WAVE_GREEN_SLOW);    break;
  case Edgent::MODE_OTA_UPGRADE:      userLED.setMode(Indicator::BLINK_MAGENTA_FAST); break;
  case Edgent::MODE_ERROR:            userLED.setMode(Indicator::HEARTBEAT_RED);      break;
  default:                            userLED.setMode(Indicator::STAY_OFF);           break;
  }
}


#if defined(BOARD_BUTTON_PIN)

#include <OneButton.h>
OneButton userBTN(BOARD_BUTTON_PIN, BOARD_BUTTON_ACTIVE_LOW);

void setupButton() {
  userBTN.setPressMs(BUTTON_HOLD_TIME_LONG_PRESS);
  userBTN.setLongPressIntervalMs(1000);

  static bool willReset = false;

  userBTN.attachLongPressStart([]() {
    LOG_I("Button: Long Press!");
  });
  userBTN.attachDuringLongPress([]() {
    uint32_t passed = userBTN.getPressedMs();
    if (passed > BUTTON_HOLD_TIME_CANCEL) {
      if (willReset) {
        willReset = false;
        updateIndicator();
        LOG_I("Config reset canceled");
      }
    } else if (passed > BUTTON_HOLD_TIME_CONFIG_RESET) {
      if (!willReset) {
        willReset = true;
        userLED.setMode(Indicator::BLINK_WHITE_FAST);
        LOG_I("Release the button to reset config!");
      }
    } else if (passed > BUTTON_HOLD_TIME_INDICATION) {
      userLED.setMode(Indicator::WAVE_WHITE_FAST);
    }
  });
  userBTN.attachLongPressStop([]() {
    uint32_t passed = userBTN.getPressedMs();
    if (passed > BUTTON_HOLD_TIME_CANCEL) {
      // Button was held for too long -> cancel
    } else if (passed > BUTTON_HOLD_TIME_CONFIG_RESET) {
      LOG_I("Resetting configuration");
      BlynkEdgent.resetConfig();
    } else {
      LOG_I("Button: Released (%d ms)", passed);
    }
    willReset = false;
    updateIndicator();
  });

  userBTN.attachClick([](){
    LOG_I("Button: Click!");
  });
  userBTN.attachDoubleClick([](){
    LOG_I("Button: Double Click!");
  });
}

void runButton() {
  userBTN.tick();
}

#else

void setupButton() {
}

void runButton() {
}

#endif

void setupInteraction()
{
  userLED.begin();

  BlynkEdgent.onStateChange([]() {
    updateIndicator();
  });

  setupButton();
}

void runInteraction() {
  runButton();
}

