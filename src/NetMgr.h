/**
 * @author     Volodymyr Shymanskyy
 * @copyright  Copyright (c) 2026 Volodymyr Shymanskyy
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NetMgr_h
#define NetMgr_h

#if defined(PARTICLE)
  #include <Particle.h>
#elif defined(ARDUINO)
  #include <Arduino.h>
  #include <IPAddress.h>
#endif

#include <NetMgrLogger.h>
#include <NetMgrUtils.h>

#if defined(ARDUINO_TTGO_TPCIE)
  #include "boards/TTGO_TPCIE.h"
#elif defined(ARDUINO_TTGO_TINET_COM)
  #include "boards/TTGO_TINET_COM.h"
#elif defined(ARDUINO_TTGO_TCALL_SIM800)
  #include "boards/TTGO_TCALL_SIM800.h"
#else
static inline void netmgrSetupBoard() {}
#endif

#if defined(NETMGR_USE_LIB_ETHERNET)
  #include <nm_arduino/NetMgrEthernet.h>
  #define NetMgr_Ethernet 1
typedef NetMgrArduinoEthernet NetMgrEthernetClass;
extern NetMgrEthernetClass NetMgrEthernet;
#endif

#if defined(NETMGR_USE_LIB_TINYGSM)
  #include <nm_arduino/NetMgrTinyGSM.h>
  #define NetMgr_Cellular 1
typedef NetMgrTinyGSM NetMgrCellularClass;
extern NetMgrCellularClass NetMgrCellular;
#endif

#if defined(ESP32)

  #include <nm_esp32/NetMgrWiFi.h>
  #define NetMgr_WiFi 1
typedef NetMgrEsp32WiFi NetMgrWiFiClass;
extern NetMgrWiFiClass NetMgrWiFi;

#elif defined(ESP8266)

  #include <nm_esp8266/NetMgrWiFi.h>
  #define NetMgr_WiFi 1
typedef NetMgrEsp8266WiFi NetMgrWiFiClass;
extern NetMgrWiFiClass NetMgrWiFi;

#elif defined(SEEED_WIO_TERMINAL)

  #include <nm_seeed/NetMgrWiFi.h>
  #define NetMgr_WiFi 1
typedef NetMgrSeeedRpcWiFi NetMgrWiFiClass;
extern NetMgrWiFiClass NetMgrWiFi;

#elif defined(PARTICLE)

  #if Wiring_WiFi
    #include <nm_particle/NetMgrWiFi.h>
    #define NetMgr_WiFi 1
typedef NetMgrParticleWiFi NetMgrWiFiClass;
extern NetMgrWiFiClass NetMgrWiFi;
  #endif
  #if Wiring_Ethernet
    #include <nm_particle/NetMgrEthernet.h>
    #define NetMgr_Ethernet 1
typedef NetMgrParticleEthernet NetMgrEthernetClass;
extern NetMgrEthernetClass NetMgrEthernet;
  #endif
  #if Wiring_Cellular
    #include <nm_particle/NetMgrCellular.h>
    #define NetMgr_Cellular 1
typedef NetMgrParticleCellular NetMgrCellularClass;
extern NetMgrCellularClass NetMgrCellular;
  #endif

#endif

// Detect a common typo in NetMgr_WiFi
#pragma GCC poison NetMgr_Wifi

struct NetworkManagerClass {

    void setHostname(const String& hostname) {
        (void)hostname;
#if defined(NetMgr_Ethernet)
        NetMgrEthernet.setHostname(hostname);
#endif
#if defined(NetMgr_WiFi)
        NetMgrWiFi.setHostname(hostname);
#endif
    }

    void begin() {
        netmgrSetupBoard();

#if defined(NetMgr_Ethernet)
        NetMgrEthernet.begin();
#endif
#if defined(NetMgr_WiFi)
        NetMgrWiFi.begin();
#endif
#if defined(NetMgr_Cellular)
        NetMgrCellular.begin();
#endif
    }

    void run() {
#ifdef NetMgr_WiFi
        NetMgrWiFi.run();
#endif
#ifdef NetMgr_Ethernet
        NetMgrEthernet.run();
#endif
#ifdef NetMgr_Cellular
        NetMgrCellular.run();
#endif
    }

    void allOn() {
#ifdef NetMgr_WiFi
        NetMgrWiFi.on();
#endif
#ifdef NetMgr_Ethernet
        NetMgrEthernet.on();
#endif
#ifdef NetMgr_Cellular
        NetMgrCellular.on();
#endif
    }

    void allOff() {
#ifdef NetMgr_WiFi
        NetMgrWiFi.off();
#endif
#ifdef NetMgr_Ethernet
        NetMgrEthernet.off();
#endif
#ifdef NetMgr_Cellular
        NetMgrCellular.off();
#endif
    }

    bool isAnyConnected() {
#ifdef NetMgr_WiFi
        if (NetMgrWiFi.isConnected()) { return true; }
#endif
#ifdef NetMgr_Ethernet
        if (NetMgrEthernet.isConnected()) { return true; }
#endif
#ifdef NetMgr_Cellular
        if (NetMgrCellular.isConnected()) { return true; }
#endif
        return false;
    }

    bool isAnyConfigured() {
#ifdef NetMgr_WiFi
        if (NetMgrWiFi.isConfigured()) { return true; }
#endif
#ifdef NetMgr_Ethernet
        if (NetMgrEthernet.isConfigured()) { return true; }
#endif
#ifdef NetMgr_Cellular
        if (NetMgrCellular.isConfigured()) { return true; }
#endif
        return false;
    }

    void clearAllNetworks() {
#ifdef NetMgr_WiFi
        NetMgrWiFi.clearNetworks();
#endif
#ifdef NetMgr_Ethernet
        NetMgrEthernet.clearNetworks();
#endif
#ifdef NetMgr_Cellular
        NetMgrCellular.clearNetworks();
#endif
    }
};

extern NetworkManagerClass NetMgr;

#endif /* NetMgr_h */
