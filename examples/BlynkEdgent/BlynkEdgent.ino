/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Fill in information from your Blynk Template here */
/* Read more: https://bit.ly/BlynkInject */
#define BLYNK_TEMPLATE_ID           "TMPxxxxxx"
#define BLYNK_TEMPLATE_NAME         "Device"

/* White labeling (use this ONLY if you have a branded Blynk App) */
//#define BLYNK_VENDOR_PREFIX         "Blynk"
//#define BLYNK_DEFAULT_SERVER        "my-dashboard.com"

/* The firmware version (used for OTA updates) */
#define BLYNK_FIRMWARE_VERSION      "0.1.0"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#include <BlynkEdgent.h>

#include "Interaction.h"

BlynkTimer timer;

/*
 * Remote Terminal
 */

#include <utility/BlynkStreamMulti.h>
MultiStream    MultiSerial;
WidgetTerminal VirtualSerial;
BLYNK_ATTACH_WIDGET(VirtualSerial, V64);

/*
 * Main
 */

void updateConnectionType() {
  Blynk.virtualWrite(V13, BlynkEdgent.getConnNetworkType());
  Blynk.sendInternal("meta", "set", "Network", BlynkEdgent.getConnNetworkName());
}

void updateRSSI() {
#ifdef NetMgr_WiFi
  Blynk.virtualWrite(V10, NetMgrWiFi.getRSSI());
#endif
}

BLYNK_CONNECTED() {
  updateConnectionType();
  updateRSSI();
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

#ifdef USE_ESP_IDF_LOG
  esp_log_level_set("*", (esp_log_level_t)CORE_DEBUG_LEVEL);
#endif

  setupInteraction();

  // Set unlimited configuration mode retries (use only for testing!!!)
  BlynkEdgent.setConfigSkipLimit(0);

  // Initialize Blynk.Edgent
  BlynkEdgent.begin();

  // Enable remote and local Edgent Console (optional)
  VirtualSerial.autoAppendLF();
  MultiSerial.addStream(BLYNK_PRINT);
  MultiSerial.addStream(VirtualSerial);
  BlynkEdgent.initConsole(MultiSerial);

  // Update RSSI every 30 seconds
  timer.setInterval(30000, updateRSSI);

  // Update connection type widget every 5 minutes
  timer.setInterval(5*60000, updateConnectionType);
}

void loop()
{
  BlynkEdgent.run();
  timer.run();
  runInteraction();
  delay(1);
}
