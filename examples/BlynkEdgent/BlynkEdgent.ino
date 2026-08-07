
/******************************************************************************
 * 1. In ArduinoIDE, select:
 *    Tools -> Board -> esp32 -> Your Board
 *    Tools -> Partition Scheme -> "RainMaker 4MB" (or 8MB)
 *    It is highly recommended to create a custom Partition Scheme
 *    optimized for your project.
 *
 * 2. In BoardConfig.h, configure the LED and button pins.
 *
 * 3. Fill in TEMPLATE_ID and TEMPLATE_NAME from your Blynk Template below.
 *    Read more: https://bit.ly/BlynkInject
 *
 *****************************************************************************/
//#define BLYNK_TEMPLATE_ID           "TMPxxxxxx"
//#define BLYNK_TEMPLATE_NAME         "Device"

/* White labeling (use this ONLY if you have a branded Blynk App) */
//#define BLYNK_VENDOR_PREFIX         "Blynk"
//#define BLYNK_DEFAULT_SERVER        "my-dashboard.com"

/* The firmware version (used for OTA updates) */
#define BLYNK_FIRMWARE_VERSION      "0.1.0"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#include <BlynkEdgent.h>
#include "Interaction.h"

void setup()
{
  Serial.begin(115200);
  Serial.println();

  interaction.begin();

  // The amount of times the board enters the config mode automatically.
  // NOTE: 0 means unlimited, and is only useful for testing. Default: 5.
  BlynkEdgent.setConfigSkipLimit(0);

  // Initialize Blynk.Edgent
  BlynkEdgent.begin();

  // Attach Blynk console to the Serial
  BlynkEdgent.initConsole(BLYNK_PRINT);
}

void loop()
{
  BlynkEdgent.run();
  interaction.run();
  delay(1);
}
