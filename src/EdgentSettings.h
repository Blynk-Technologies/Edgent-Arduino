/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#if defined(PARTICLE)
  #include <Particle.h>
#else
  #include <Arduino.h>
#endif

#define CONFIG_COMMAND_SYS
#define CONFIG_COMMAND_NETMGR
//#define CONFIG_COMMAND_FILESYS
//#define CONFIG_COMMAND_I2CDETECT
//#define CONFIG_COMMAND_PREFS

#if defined(ESP32)
  #define BLYNK_MULTITHREADED
  #define CONFIG_USE_SSL
  #if CONFIG_BT_BLE_ENABLED || CONFIG_BT_NIMBLE_ENABLED
    // Boards with BLE hardware use BLE-assisted provisioning (via NimBLE-Arduino).
    #define CONFIG_USE_INJECT_BLE
    #define CONFIG_USE_OTA_BLE
  #else
    #define CONFIG_USE_INJECT_WIFIAP
  #endif
#elif defined(ESP8266)
  #define CONFIG_USE_SSL
  #define CONFIG_USE_INJECT_WIFIAP
#elif defined(SEEED_WIO_TERMINAL)
  #define CONFIG_USE_SSL
  #define CONFIG_USE_INJECT_BLE
  #define CONFIG_USE_OTA_BLE
#elif defined(PARTICLE)
  // #define CONFIG_USE_SSL - not supported on Particle devices
  #define CONFIG_USE_INJECT_BLE
#endif

#if !defined(BLYNK_VENDOR_PREFIX)
  #if defined(BLYNK_DEVICE_PREFIX)
    // Backward compatibility with the old macro
    #define BLYNK_VENDOR_PREFIX BLYNK_DEVICE_PREFIX
  #else
    #define BLYNK_VENDOR_PREFIX "Blynk"
  #endif
#endif
#if !defined(BLYNK_DEFAULT_SERVER)
  #define BLYNK_DEFAULT_SERVER "blynk.cloud"
#endif

// Disable built-in analog and digital pin control
#define BLYNK_NO_BUILTIN
#define BLYNK_NO_DEFAULT_BANNER
#define BLYNK_SEND_ATOMIC

// If max connection retries is exceeded, device enters ERROR state and reboots
#if !defined(WIFI_CLOUD_MAX_RETRIES)
  #define WIFI_CLOUD_MAX_RETRIES 500
#endif
#if !defined(WIFI_NET_CONNECT_TIMEOUT)
  #define WIFI_NET_CONNECT_TIMEOUT 50000     // ms
#endif
#if !defined(WIFI_CLOUD_CONNECT_TIMEOUT)
  #define WIFI_CLOUD_CONNECT_TIMEOUT 50000     // ms
#endif
