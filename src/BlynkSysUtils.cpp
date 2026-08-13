/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <BlynkSysUtils.h>
#include <Blynk/BlynkUtility.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_AMEBAD)
  #include <WiFi.h>
#elif defined(ARDUINO_ARCH_SAMD)
  #if defined(__SAMD51__)
    #define SERIAL_NUMBER_WORD_0 *(volatile uint32_t *)(0x008061FC)
    #define SERIAL_NUMBER_WORD_1 *(volatile uint32_t *)(0x00806010)
    #define SERIAL_NUMBER_WORD_2 *(volatile uint32_t *)(0x00806014)
    #define SERIAL_NUMBER_WORD_3 *(volatile uint32_t *)(0x00806018)
  #else
    #define SERIAL_NUMBER_WORD_0 *(volatile uint32_t *)(0x0080A00C)
    #define SERIAL_NUMBER_WORD_1 *(volatile uint32_t *)(0x0080A040)
    #define SERIAL_NUMBER_WORD_2 *(volatile uint32_t *)(0x0080A044)
    #define SERIAL_NUMBER_WORD_3 *(volatile uint32_t *)(0x0080A048)
  #endif
#elif defined(ARDUINO_ARCH_RP2040)
  #include <pico/unique_id.h>
  #include <hardware/watchdog.h>
  #include <hardware/regs/vreg_and_chip_reset.h>
  #include <hardware/structs/vreg_and_chip_reset.h>
  #include <hardware/structs/watchdog.h>
#elif defined(PARTICLE)
  #include "dct.h"
#endif

BLYNK_NOINIT_ATTR
SystemStats systemStats;

static String sysDevPrefix = "Unknown", sysDevName = "Device";

void systemInit(String devPrefix, String devName)
{
  static bool initialized = false;
  if (!initialized) {
    sysDevPrefix = devPrefix;
    sysDevName   = devName;

#if defined(BLYNK_USE_LITTLEFS)
  #if defined(ESP32)
    // Find a suitable partition based on TYPE and SUBTYPE
    const esp_partition_t* lfs_pt = esp_partition_find_first(
                ESP_PARTITION_TYPE_DATA,
                ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    if (lfs_pt) {
      LittleFS.begin(true, "/lfs", 5, lfs_pt->label);
    }
  #elif defined(ESP8266)
    LittleFS.begin();
  #else
    #error "Platform not supported"
  #endif
#elif defined(BLYNK_USE_SPIFFS)
    SPIFFS.begin(true);
#endif

#if defined(PARTICLE)
    System.enableFeature(FEATURE_RESET_INFO);

    #if defined(DCT_SETUP_DONE_OFFSET) // && !defined(SYSTEM_VERSION_v400ALPHA1)
      // On Gen3 devices, set the setup done flag to true so the device exits
      // listening mode. This happens immediately on cellular devices or after
      // valid Wi-Fi credentials have been set on the Argon.
      // This is only necessary with Device OS 3.x and earlier.
      uint8_t val = 0;
      if(!dct_read_app_data_copy(DCT_SETUP_DONE_OFFSET, &val, sizeof(val)) && val != 1)
      {
        val = 1;
        dct_write_app_data(&val, DCT_SETUP_DONE_OFFSET, sizeof(val));
      }
    #endif
#endif  // PARTICLE

    initialized = true;
  }
}

String timeSpanToStr(const uint64_t t) {
  unsigned secs = t % 60;
  unsigned mins = (t / 60) % 60;
  unsigned hrs  = (t % (24UL*3600UL)) / 3600UL;
  unsigned days = (t / (24UL*3600UL));

  char buff[32];
  snprintf(buff, sizeof(buff), "%dd %dh %dm %ds", days, hrs, mins, secs);
  return buff;
}

static
String encodeUniquePart(uint32_t n, unsigned len)
{
  /*
   *  === Base 24 alphabet selection ===
   *
   *  Sorted:     0 1 2 3 4 5 6 7 8 9 A C D F H J K M N P R W X Y
   *  Shuffled:   0 W 8 N 4 Y 1 H P 5 D F 9 K 6 J M 3 C 2 X A 7 R
   *  Excluded:
   *    B - similar to 8
   *    E - similar to F
   *    G - similar to 6
   *    I - similar to 1,i,l
   *    L - similar to 1,i,I (when lowercase)
   *    O - similar to 0,Q
   *    Q - similar to 0,O
   *    S - similar to 5
   *    T - similar to 7
   *    U - accidental obscenity
   *    V,Z - unneded
   *
   * Interesting read: https://www.crockford.com/base32.html
   */
  static constexpr char alphabet[] = { "0W8N4Y1HP5DF9K6JM3C2XA7R" };
  static constexpr int base = sizeof(alphabet)-1;

  char buf[16] = { 0, };
  char prev = 0;
  for (unsigned i = 0; i < len; n /= base) {
    char c = alphabet[n % base];
    if (c == prev) {
      c = alphabet[(n+1) % base];
    }
    prev = buf[i++] = c;
  }
  return String(buf);
}

static
String getDeviceRandomSuffix(unsigned size)
{
  static uint32_t unique = 0;
  static bool hasUnique = false;
  if (!hasUnique) {
#if defined(ESP32)
    const uint64_t chipId = ESP.getEfuseMac();
#elif defined(ESP8266) || defined(ARDUINO_ARCH_AMEBAD)
    uint8_t chipId[6] = { 0, };
    WiFi.macAddress(chipId);
#elif defined(PARTICLE)
    char chipId[24];
    String deviceID = System.deviceID();
    strncpy(chipId, deviceID.c_str(), sizeof(chipId));
#elif defined(ARDUINO_ARCH_SAMD)
    const uint32_t chipId[4] = {
      SERIAL_NUMBER_WORD_0,
      SERIAL_NUMBER_WORD_1,
      SERIAL_NUMBER_WORD_2,
      SERIAL_NUMBER_WORD_3
    };
#elif defined(ARDUINO_ARCH_RP2040)
    pico_unique_board_id_t chipId;
    pico_get_unique_board_id(&chipId);

#elif defined(ARDUINO_ARCH_NRF5)
    const uint32_t chipId[2] = {
      NRF_FICR->DEVICEID[1],
      NRF_FICR->DEVICEID[0]
    };
#else
    #warning "Platform not supported"
    const uint32_t chipId = 0;
    return "0000";
#endif

    for (int i=0; i<4; i++) {
      unique = BlynkCRC32(&chipId, sizeof(chipId), unique);
    }
    hasUnique = true;
  }
  return encodeUniquePart(unique, size);
}

String systemGetDeviceName(bool withPrefix/* = true*/)
{
  String devUnique = getDeviceRandomSuffix(4);
  const int maxTmplName = 29 - (2 + sysDevPrefix.length() + devUnique.length());
  String devName = sysDevName.substring(0, maxTmplName);
  if (withPrefix)
  {
    // Use short name if devPrefix and devName are the same
    if (sysDevPrefix == devName)
    {
      return sysDevPrefix + "-" + devUnique;
    }
    else
    {
      return sysDevPrefix + " " + devName + "-" + devUnique;
    }
  }
  else
  {
    return devName + "-" + devUnique;
  }
}

uint64_t systemUptime()
{
#if defined(ESP32)
  return esp_timer_get_time() / 1000;
#elif defined(ESP8266)
  return micros64() / 1000;
#elif defined(PARTICLE)
  return System.millis();
#else
  // TODO: track overflow
  return millis();
#endif
}

void systemReboot()
{
  systemStats.resetCount.graceful++;
  delay(50);
#if defined(ESP32) || defined(ESP8266)
  ESP.restart();
#elif defined(ARDUINO_ARCH_SAMD) || \
      defined(ARDUINO_ARCH_SAM)  || \
      defined(ARDUINO_ARCH_RENESAS) || \
      defined(ARDUINO_ARCH_NRF5) || \
      defined(ARDUINO_ARCH_AMEBAD)
  NVIC_SystemReset();
#elif defined(ARDUINO_ARCH_RP2040)
  rp2040.reboot();
#elif defined(PARTICLE)
  System.reset();
#else
  #error "Platform not supported"
#endif
  while(1) {};
}

/***************************************************
 * systemGetResetReason()
 ***************************************************/

#if defined(ESP32)

#ifdef ESP_IDF_VERSION_MAJOR    // IDF 4+
  #if CONFIG_IDF_TARGET_ESP32   // ESP32/PICO-D4
    #include "esp32/rom/rtc.h"
  #elif CONFIG_IDF_TARGET_ESP32S2
    #include "esp32s2/rom/rtc.h"
  #elif CONFIG_IDF_TARGET_ESP32C3
    #include "esp32c3/rom/rtc.h"
  #elif CONFIG_IDF_TARGET_ESP32C6
    #include "esp32c6/rom/rtc.h"
  #elif CONFIG_IDF_TARGET_ESP32C5
    #include "esp32c5/rom/rtc.h"
  #elif CONFIG_IDF_TARGET_ESP32S3
    #include "esp32s3/rom/rtc.h"
  #else
    #error "Target is not supported"
  #endif
#else                           // ESP32 Before IDF 4.0
  #include "rom/rtc.h"
#endif

String systemGetResetReason() {
  int reason = rtc_get_reset_reason(0);
  switch (reason) {
    case  1: return "POWERON_RESET"; break;          /**<1,  Vbat power on reset*/
    case  3: return "SW_RESET"; break;               /**<3,  Software reset digital core*/
    case  4: return "OWDT_RESET"; break;             /**<4,  Legacy watch dog reset digital core*/
    case  5: return "DEEPSLEEP_RESET"; break;        /**<5,  Deep Sleep reset digital core*/
    case  6: return "SDIO_RESET"; break;             /**<6,  Reset by SLC module, reset digital core*/
    case  7: return "TG0WDT_SYS_RESET"; break;       /**<7,  Timer Group0 Watch dog reset digital core*/
    case  8: return "TG1WDT_SYS_RESET"; break;       /**<8,  Timer Group1 Watch dog reset digital core*/
    case  9: return "RTCWDT_SYS_RESET"; break;       /**<9,  RTC Watch dog Reset digital core*/
    case 10: return "INTRUSION_RESET"; break;        /**<10, Instrusion tested to reset CPU*/
    case 11: return "TGWDT_CPU_RESET"; break;        /**<11, Time Group reset CPU*/
    case 12: return "SW_CPU_RESET"; break;           /**<12, Software reset CPU*/
    case 13: return "RTCWDT_CPU_RESET"; break;       /**<13, RTC Watch dog Reset CPU*/
    case 14: return "EXT_CPU_RESET"; break;          /**<14, for APP CPU, reseted by PRO CPU*/
    case 15: return "RTCWDT_BROWN_OUT_RESET"; break; /**<15, Reset when the vdd voltage is not stable*/
    case 16: return "RTCWDT_RTC_RESET"; break;       /**<16, RTC Watch dog reset digital core and rtc module*/
    default: return "<unknown>";
  }
}

#elif defined(ESP8266)

String systemGetResetReason() {
  return ESP.getResetReason();
}

#elif defined(ARDUINO_ARCH_RP2040)

String systemGetResetReason() {
  String reason;

  uint32_t chip_reset_reg = vreg_and_chip_reset_hw->chip_reset;

  if (chip_reset_reg & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_PSM_RESTART_BITS) {
      reason = "RESCUE_DEBUG";
  }
  if (chip_reset_reg & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_RUN_BITS) {
      reason = "RESET_PIN";
  }
  if (chip_reset_reg & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_POR_BITS) {
      // NOTE: This register is also used for brownout, but there is no way to differentiate between power on and brown out
      reason = "POWER_ON";
  }

  // Check watchdog after chip reset since watchdog doesn't clear chip_reset, while chip_reset clears the watchdog
  // The watchdog is used for software reboots such as resetting after copying a UF2 via the bootloader.
  if (watchdog_caused_reboot()) {
      reason = "SOFTWARE";
  }

  // Actual watchdog usage will set a special value that this function detects.
  if (watchdog_enable_caused_reboot()) {
      reason = "WATCHDOG";
  }

  return reason;
}

#elif defined(PARTICLE)

String systemGetResetReason() {
  int reason = System.resetReason();
  switch (reason) {
    // Hardware
    case RESET_REASON_PIN_RESET:        return "Reset button";
    case RESET_REASON_POWER_MANAGEMENT: return "Low-power management reset";
    case RESET_REASON_POWER_DOWN:       return "Power-down reset";
    case RESET_REASON_POWER_BROWNOUT:   return "Brownout reset";
    case RESET_REASON_WATCHDOG:         return "Hardware watchdog reset";
    // Software
    case RESET_REASON_UPDATE:           return "Successful firmware update";
    case RESET_REASON_UPDATE_TIMEOUT:   return "Firmware update timeout";
    case RESET_REASON_FACTORY_RESET:    return "Factory reset requested";
    case RESET_REASON_SAFE_MODE:        return "Safe mode requested";
    case RESET_REASON_DFU_MODE:         return "DFU mode requested";
    case RESET_REASON_PANIC:            return "System panic";
    case RESET_REASON_USER:             return "User-requested reset";
    // Other
    case RESET_REASON_UNKNOWN:          return "Unspecified reset reason";
    case RESET_REASON_NONE:             return "N/A";
    default:                            return String::format("Unknown (%d)", reason);
  }
}

#else

String systemGetResetReason() {
  return "<unknown>";
}

#endif


/***************************************************
 * systemGetDeviceUID()
 ***************************************************/

#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_AMEBAD)

#include <Preferences.h>

// This generates a random ID.
// Note: It will change each time device flash is erased.
String systemGetDeviceUID() {
  static String result;
  if (!result.length()) {
    Preferences prefs;
    if (!prefs.begin("system")) {
      return "none";
    }
    if (prefs.isKey("uid")) {
      result = prefs.getString("uid");
    }
    if (!result.length()) {
      // Generate 16 random bytes
      uint8_t uuid[16];
#if defined(ESP32)
      for (unsigned i = 0; i < sizeof(uuid); i += 4) {
          uint32_t n = esp_random();
          memcpy(&uuid[i], &n, 4);
      }
#elif defined(ESP8266)
      for (unsigned i = 0; i < sizeof(uuid); i += 4) {
          uint32_t n = ESP.random();
          memcpy(&uuid[i], &n, 4);
      }
#elif defined(ARDUINO_ARCH_AMEBAD)
      rtw_get_random_bytes(uuid, sizeof(uuid));
#else
      #error "UUID must be random"
#endif

      // Set the UUID version to 4
      uuid[6] = (uuid[6] & 0x0F) | 0x40;
      // Set the two most significant bits of the 8th byte to 10 (UUID variant 1)
      uuid[8] = (uuid[8] & 0x3F) | 0x80;

      // Convert to string format
      char str[38];
      snprintf(str, sizeof(str),
              "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
              uuid[0],  uuid[1], uuid[2], uuid[3],
              uuid[4],  uuid[5],
              uuid[6],  uuid[7],
              uuid[8],  uuid[9],
              uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
      result = str;
      if (!prefs.putString("uid", result)) {
        result = "";
        return "none";
      }
    }
  }
  return result;
}

#elif defined(PARTICLE)

String systemGetDeviceUID() {
  return System.deviceID();
}

#elif defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_NRF5) || defined(ARDUINO_ARCH_RP2040)

#include <digests/DigestEngines.h>

String systemGetDeviceUID() {
  static String result;
  if (!result.length()) {
    if (!result.length()) {
      // Get unique Device ID
#if defined(ARDUINO_ARCH_SAMD)
      uint32_t uid[4] = {
        SERIAL_NUMBER_WORD_0,
        SERIAL_NUMBER_WORD_1,
        SERIAL_NUMBER_WORD_2,
        SERIAL_NUMBER_WORD_3
      };
#elif defined(ARDUINO_ARCH_NRF5)
      uint32_t uid[2] = {
        NRF_FICR->DEVICEID[1],
        NRF_FICR->DEVICEID[0]
      };
#elif defined(ARDUINO_ARCH_RP2040)
      pico_unique_board_id_t uid;
      pico_get_unique_board_id(&uid);
#else
      #warning "Platform not supported"
      const uint32_t uid = 0;
#endif

      DigestEngineSHA256 md;
      for (int i=0; i<4; i++) {
        md.update(uid, sizeof(uid));
      }
      uint8_t uuid[32];
      md.getDigestBuffer(uuid);

      // Set the UUID version to 4
      uuid[6] = (uuid[6] & 0x0F) | 0x40;
      // Set the two most significant bits of the 8th byte to 10 (UUID variant 1)
      uuid[8] = (uuid[8] & 0x3F) | 0x80;

      // Convert to string format
      char str[38];
      snprintf(str, sizeof(str),
              "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
              uuid[0],  uuid[1], uuid[2], uuid[3],
              uuid[4],  uuid[5],
              uuid[6],  uuid[7],
              uuid[8],  uuid[9],
              uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
      result = str;
    }
  }
  return result;
}

#else

String systemGetDeviceUID() {
  return "<unknown>";
}

#endif

/***************************************************
 * systemGetFlashMode()
 ***************************************************/

#if defined(ESP32) || defined(ESP8266)

String systemGetFlashMode() {
  switch (ESP.getFlashChipMode()) {
    case FM_QIO:       return "QIO";
    case FM_QOUT:      return "QOUT";
    case FM_DIO:       return "DIO";
    case FM_DOUT:      return "DOUT";
#if defined(ESP32)
    case FM_FAST_READ: return "FAST_READ";
    case FM_SLOW_READ: return "SLOW_READ";
#endif
    case FM_UNKNOWN:   return "UNKNOWN";
    default :          return "<unknown>";
  }
}

#else

String systemGetFlashMode() {
  return "<unknown>";
}

#endif
