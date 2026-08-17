/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#if !defined(BLYNK_TEMPLATE_ID) || !defined(BLYNK_TEMPLATE_NAME)
  #error "Please specify your BLYNK_TEMPLATE_ID and BLYNK_TEMPLATE_NAME. Read more: https://bit.ly/BlynkInject"
#endif

#if defined(BLYNK_AUTH_TOKEN)
  #error "BLYNK_AUTH_TOKEN configured in runtime, please remove it from static configuration"
#endif

#include <EdgentSettings.h>
#include <NetMgr.h>
#include <BlynkNetMgrClients.h>
#include <inject/BlynkInject.h>
#include <BlynkSysUtils.h>
#include <Blynk/BlynkConsole.h>
#include <BlynkConfigStore.h>
#include <BlynkHttpOTA.h>
#include <BlynkMigrateConfig.h>

class Edgent {

public:

  typedef void (*callback0_t)(void);

  enum State {
    MODE_IDLE,
    MODE_WAIT_CONFIG,
    MODE_CONNECTING_NET,
    MODE_CONNECTING_CLOUD,
    MODE_RUNNING,
    MODE_OTA_UPGRADE,
    MODE_RESET_CONFIG,
    MODE_ERROR,

    MODE_MAX_VALUE
  };

  static const char* getStateName(State m) {
    if (m > MODE_MAX_VALUE) return "Unknown";
    static const char* stateStr[MODE_MAX_VALUE+1] = {
      "IDLE",
      "WAIT CONFIG",
      "CONNECTING NET",
      "CONNECTING CLOUD",
      "RUNNING",
      "OTA UPGRADE",
      "RESET CONFIG",
      "ERROR",

      "INIT"
    };
    return stateStr[m];
  }

  const char* getStateName() {
    return getStateName(_state);
  }

  void setConfigTimeout(int timeout) {
    _configTimeoutMs = BlynkMathClamp(timeout, 60, 3600) * 1000;
  }

  void setConfigSkipLimit(int count) {
    if (count == 0) {
      _configSkipLimit = 0;
    } else {
      _configSkipLimit = BlynkMathClamp(count, 5, 50);
    }
  }

  void setDeviceNameSuffix(const String& name) {
    systemSetNameSuffix(name);
  }

  bool begin()
  {
    if (!String(BLYNK_TEMPLATE_ID).startsWith("TMPL")) {
      LOG_E("Invalid Template configuration");
      return false;
    }

    systemInit(BLYNK_VENDOR_PREFIX, BLYNK_TEMPLATE_NAME);

    NetMgr.setHostname(systemGetFullName());
    NetMgr.begin();

    setupNetMgrClients();

    migrateOldConfig(_store);

    _store.begin();
    printBanner();
    initConsoleCommands();

    if (isConfigured()) {
      setState(MODE_CONNECTING_NET);
    } else if (_configSkipLimit &&
               (_store.getConfigSkipped() >= int(_configSkipLimit)))
    {
      setState(MODE_IDLE);
    } else {
      setState(MODE_WAIT_CONFIG);
    }

    return true;
  }

  void initConsole(Stream& stream);

  void run() {
    _timer.run();
    _console.run();
    _inject.run();
    if (_inject.isAppDisconnected()) {
      _inject.setUserFinishedConfiguring();
      if (_state == MODE_RUNNING) {
        _timer.setTimeout(1000L, [this]() {
          _inject.end();
        });
      } else if (_state == MODE_CONNECTING_NET || _state == MODE_CONNECTING_CLOUD) {
        LOG_W("App disconnected, trying to connect anyway...");
        _timer.setTimeout(1000L, [this]() {
          _inject.end();
        });
      } else {
        restartProvisioning();
      }
    }

    NetMgr.run();

    switch (_state) {
    case MODE_IDLE:             stateIdle();              break;
    case MODE_WAIT_CONFIG:      stateConfig();            break;
    case MODE_CONNECTING_NET:   stateConnectingNet();     break;
    case MODE_CONNECTING_CLOUD: stateConnectingCloud();   break;
    case MODE_RUNNING:          stateRunning();           break;
    case MODE_OTA_UPGRADE:      stateOTA();               break;
    case MODE_RESET_CONFIG:     stateResetConfig();       break;
    default:                    stateError();             break;
    }
  }

  const String& getConnNetworkType() const {
    return _blynkTransport.getConnNetworkType();
  }

  const String getConnNetworkName() const {
    const String& net = getConnNetworkType();
#ifdef NetMgr_WiFi
    if (net == "wifi") { return String("WiFi: ") + NetMgrWiFi.getNetworkSSID();  }
#endif
#ifdef NetMgr_Cellular
    if (net == "cell") { return String("Cell: ") + NetMgrCellular.getOperator(); }
#endif
#ifdef NetMgr_Ethernet
    if (net == "eth")  { return "Ethernet"; }
#endif
    return "None";
  }

private:

  void initConsoleCommands();

  /*
   * States
   */

  void stateConfig() {
    if (isEnteringState()) {
      _inject._config.host = BLYNK_DEFAULT_SERVER;

      _inject.setProvisionCallback(provisionCb);
      _inject.begin(systemGetFullName(),
                    BLYNK_VENDOR_PREFIX,
                    BLYNK_TEMPLATE_ID,
                    BLYNK_FIRMWARE_TYPE,
                    BLYNK_FIRMWARE_VERSION);

      setStateEntered();
    }
    if (millis() - _stateChangeTime > _configTimeoutMs) {
      if (_inject.isUserConfiguring()) {
        _stateChangeTime = millis(); // restart timer
        return;
      }
      // Write to NVM only if configSkipLimit is in use
      if (_configSkipLimit) {
        _store.storeConfigSkipped();
      }
      stopConfig();
    }
  }

  void stateIdle() {
    if (isEnteringState()) {
      if (_prevState == MODE_WAIT_CONFIG) {
        _inject.end();
      }
      // TODO: disable NetMgr?

      setStateEntered();
    }
  }

  void stateConnectingNet() {
    if (isEnteringState()) {
      _inject.reportStatus(BlynkInject::STATUS_CONNECTING_NETWORK);
      NetMgr.allOn();
      setStateEntered();
    }

    if (NetMgr.isAnyConnected()) {
      _retriesNet = WIFI_CLOUD_MAX_RETRIES;
      setState(MODE_CONNECTING_CLOUD);
    } else if (millis() - _stateChangeTime > WIFI_NET_CONNECT_TIMEOUT) {
      LOG_E("Network connection timeout");
      if (--_retriesNet <= 0) {
        const auto netError = analyzeNetworkError();
        _inject.reportFailure(netError);

        // If setting not saved -> return to config mode
        if (!_store.isSaved()) {
          restartProvisioning();
        } else {
          setState(MODE_ERROR);
        }
      } else {
        setState(MODE_CONNECTING_NET, true);
      }
    }
  }

  BlynkInject::InjectError analyzeNetworkError() {
#ifdef NetMgr_WiFi
    if (_inject._config.intf == "wifi") {
      switch (NetMgrWiFi.getError()) {
      case NetMgrWiFi.NETMGR_ERR_NETWORK_NOT_FOUND:  return BlynkInject::ERROR_NETWORK_NOT_FOUND;
      case NetMgrWiFi.NETMGR_ERR_AUTH_FAILED:        return BlynkInject::ERROR_NETWORK_AUTH_FAIL;
      case NetMgrWiFi.NETMGR_ERR_IP_NOT_ASSIGNED:    return BlynkInject::ERROR_NETWORK_NO_ADDRESS;
      default: break;
      }
    }
#endif
#ifdef NetMgr_Ethernet
    if (_inject._config.intf == "eth") {
      switch (NetMgrEthernet.getError()) {
      case NetMgrEthernet.NETMGR_ERR_NO_CABLE:           return BlynkInject::ERROR_NETWORK_NO_CABLE;
      case NetMgrEthernet.NETMGR_ERR_IP_NOT_ASSIGNED:    return BlynkInject::ERROR_NETWORK_NO_ADDRESS;
      default: break;
      }
    }
#endif
#ifdef NetMgr_Cellular
    if (_inject._config.intf == "cell") {
      switch (NetMgrCellular.getError()) {
      case NetMgrCellular.NETMGR_ERR_NO_NETWORK:           return BlynkInject::ERROR_NETWORK_NOT_FOUND;
      case NetMgrCellular.NETMGR_ERR_DATA_NOT_CONNECTED:   return BlynkInject::ERROR_NETWORK_NO_ADDRESS;
      case NetMgrCellular.NETMGR_ERR_SIM_MISSING:          return BlynkInject::ERROR_SIMCARD_MISSING;
      case NetMgrCellular.NETMGR_ERR_SIM_LOCKED:           return BlynkInject::ERROR_SIMCARD_LOCKED;
      case NetMgrCellular.NETMGR_ERR_SIM_INVALID_PIN:      return BlynkInject::ERROR_SIMCARD_WRONG_PIN;
      default: break;
      }
    }
#endif

    return BlynkInject::ERROR_NETWORK_TIMEOUT;
  }

  void restartProvisioning() {
    _isTokenInvalid = false;
    _inject.clearRuntimeConfig();
    setState(MODE_WAIT_CONFIG);
  }

  void stateConnectingCloud() {
    if (isEnteringState()) {
      _inject.reportNetStatus();
      _inject.reportStatus(BlynkInject::STATUS_CONNECTING_CLOUD);
#if defined(PARTICLE)
      Particle.connect();
#endif

      Blynk.config(_store.getBlynkAuth().c_str(),
                   _store.getBlynkHost().c_str());
      Blynk.connect(0); // Start connecting, wait 0ms

      setStateEntered();
    }

    Blynk.run();

    if (Blynk.connected()) {
      if (!_store.isSaved()) {
        _store.commit();
        if (!_store.isSaved()) {
          _inject.reportFailure(BlynkInject::ERROR_CONFIG, "failed to store configuration");
          restartProvisioning();
          return;
        }
        _inject.reportStatus(BlynkInject::STATUS_CONNECTED);
        LOG_I("Config saved.");

        if (_onInitialConnection) { _onInitialConnection(); }
      }
      _retriesCloud = WIFI_CLOUD_MAX_RETRIES;
      systemStats.trackConnected();
      setState(MODE_RUNNING);

      if (_onStartupConnection) {
        String curr_fw = BLYNK_FIRMWARE_VERSION;
        String prev_fw = _store.getFirmwareVer();
        if (curr_fw != prev_fw) {
          if (prev_fw.length()) {
            Blynk.logEvent("sys_ota", String("Firmware updated from ") + prev_fw + " to " + curr_fw);
          }
          _store.storeFirmwareVer(curr_fw);
        }

        Blynk.sendInternal("meta", "set", "Device UID",   systemGetDeviceUID());
        Blynk.sendInternal("meta", "set", "Hotspot Name", systemGetFullName());

        _timer.setTimeout(10000L, [this]() {
          if (size_t dumpSize = systemCoreDumpSize()) {
            const String srv = _blynkTransport.getServerDomain();
            LOG_W("System crash detected. Uploading core dump (%d bytes) to %s", dumpSize, srv.c_str());
            if (!uploadCoreDump(srv, _store.getBlynkAuth(), dumpSize)) {
              LOG_E("Core dump upload failed");
            }
          }
        });

        if (_onStartupConnection != (callback0_t)1) {
          _onStartupConnection();
        }
        _onStartupConnection = NULL;
      }

    } else if ((_isTokenInvalid = Blynk.isTokenInvalid())) {
      if (!_store.isSaved()) {
        _inject.reportFailure(BlynkInject::ERROR_CLOUD_TOKEN);
      }
      restartProvisioning();  // TODO: retry after timeout
    } else if (!NetMgr.isAnyConnected()) {
      setState(MODE_CONNECTING_NET);
    } else if (millis() - _stateChangeTime > WIFI_CLOUD_CONNECT_TIMEOUT) {
      LOG_E("Cloud connection timeout");
      if (--_retriesCloud <= 0) {
        // TODO: analyze reason
        _inject.reportFailure(BlynkInject::ERROR_CLOUD_TIMEOUT);

        // If setting not saved -> return to config mode
        if (!_store.isSaved()) {
          restartProvisioning();
        } else {
          setState(MODE_ERROR);
        }
      } else {
        setState(MODE_CONNECTING_CLOUD, true);
      }
    }
  }

  void stateRunning() {
    if (!Blynk.connected()) {
      systemStats.trackDisconnected();
      if (NetMgr.isAnyConnected()) {
        systemStats.cloud_drops++;
        setState(MODE_CONNECTING_CLOUD);
      } else {
        systemStats.network_drops++;
        setState(MODE_CONNECTING_NET);
      }
    }
    Blynk.run();
  }

  void stateOTA() {
    if (!downloadUpgrade(_otaUrl)) {
      setState(MODE_ERROR);
    }
  }

  void stateResetConfig() {
    LOG_W("Resetting configuration!");
    _store.erase();
    NetMgr.clearAllNetworks();
    setState(MODE_WAIT_CONFIG);
  }

  void stateError() {
    if (millis() - _stateChangeTime > 10000) {
      LOG_E("Restarting after error.");
      systemReboot();
    }
  }

public:

  void onStateChange(callback0_t f) {
    _onStateChange = f;
  }

  void onInitialConnection(callback0_t f) {
    _onInitialConnection = f;
  }

  void onStartupConnection(callback0_t f) {
    _onStartupConnection = f;
  }

  void onUserInitiatedReboot(callback0_t f) {
    _onUserInitiatedReboot = f;
  }

  void onConfigChange(callback0_t f) {
    _onConfigChange = f;
  }

  State getState() { return _state; }

  void setState(State m, bool reenter = false) {
    if (m >= MODE_MAX_VALUE) return;

    if (_state != m || reenter) {
      LOG_I("%s => %s", getStateName(_state), getStateName(m));
      _prevState = (reenter) ? MODE_MAX_VALUE : _state;
      _state = m;
      _stateChangeTime = millis();

      if (_onStateChange) { _onStateChange(); }
    }
  }

  void startConfig() {
    Blynk.disconnect();
    setState(MODE_WAIT_CONFIG);
  }

  void stopConfig() {
    if (_store.isConfigured() && !_isTokenInvalid) {
      setState(MODE_CONNECTING_NET);
    } else {
      NetMgr.allOff();
      setState(MODE_IDLE);
    }
  }

  void setAuthToken(String auth) {
    _store.setBlynkAuth(auth);
    _store.commit();
    setState(MODE_CONNECTING_NET);
  }

  bool isConfigured() {
    return _store.isConfigured() && NetMgr.isAnyConfigured();
  }

  void startInitialConnection() {
    _isTokenInvalid = false;
    _retriesNet = _retriesCloud = 1;
    setState(MODE_CONNECTING_NET);
  }

  void startOTA(const String& url) {
    _otaUrl = url;

#if defined(ESP8266) || defined(SEEED_WIO_TERMINAL)
    // Use HTTP by default
    if (!_otaUrl.endsWith("&s=1")) {
       _otaUrl.replace("https://", "http://");
    }
#elif defined(CONFIG_USE_SSL)
    // Use HTTPS by default
    if (!_otaUrl.endsWith("&s=0")) {
       _otaUrl.replace("http://", "https://");
    }
#endif

    _timer.setTimeout(1000L, [this](){
      // Disconnect, not to interfere with OTA process
      Blynk.disconnect();

      setState(MODE_OTA_UPGRADE);
    });
  }

  void resetConfig() {
    if (getState() != MODE_WAIT_CONFIG) {
      setState(MODE_RESET_CONFIG);
    }
  }

  BlynkConsole&         getConsole()      { return _console; }
  BlynkInject::Config&  getInjectConfig() { return _inject._config; }

private:

  static void provisionCb();

  void provisioned() {
    if (_inject._config.intf == "wifi") {
#ifdef NetMgr_WiFi
      if (_inject._config.ip.length()) {
        NetMgrWiFi.addNetwork(_inject._config.ssid, _inject._config.pass, _inject._config.ip,
          _inject._config.gw, _inject._config.mask, _inject._config.dns, _inject._config.dns2);
      } else {
        NetMgrWiFi.addNetwork(_inject._config.ssid, _inject._config.pass);
      }
#endif
    }
    _store.setBlynkHost(_inject._config.host);
    _store.setBlynkAuth(_inject._config.auth);

    if (_inject._config.forceSave) {
      // Store the configuration without waiting for a successful connection
      _store.commit();
      if (!_store.isSaved()) {
        LOG_E("Failed to store configuration");
      }
    }

    if (_onConfigChange) { _onConfigChange(); }

    startInitialConnection();
  }

  void printBanner()
  {
#ifdef BLYNK_PRINT
    Blynk.printBanner();
    BLYNK_PRINT.printf("----------------------------------------------------\n");
    BLYNK_PRINT.printf(" Device:    %s\n", systemGetFullName().c_str());
    BLYNK_PRINT.printf(" Firmware:  %s (build %s)\n", BLYNK_FIRMWARE_VERSION, __DATE__ " " __TIME__);
    BLYNK_PRINT.printf(" UID:       %s\n", systemGetDeviceUID().c_str());
    if (_store.isConfigured()) {
      BLYNK_PRINT.printf(" Token:     %s - •••• - •••• - ••••\n", _store.getBlynkAuth().substring(0,4).c_str());
    }
    BLYNK_PRINT.printf(" Platform:  %s\n", BLYNK_INFO_DEVICE);
    BLYNK_PRINT.printf("----------------------------------------------------\n");
#endif
  }

private:

  BlynkTimer    _timer;
  BlynkConsole  _console;
  BlynkInject   _inject;
  ConfigStore   _store;

  uint32_t      _stateChangeTime = 0;
  State         _state          = MODE_MAX_VALUE;
  State         _prevState      = MODE_MAX_VALUE;

  int           _retriesNet     = WIFI_CLOUD_MAX_RETRIES;
  int           _retriesCloud   = WIFI_CLOUD_MAX_RETRIES;
  unsigned      _configTimeoutMs = 10*60*1000;
  unsigned      _configSkipLimit = 10;
  bool          _isTokenInvalid = false;

  String        _otaUrl;

  callback0_t   _onStateChange       = NULL;
  callback0_t   _onInitialConnection = NULL;
  callback0_t   _onStartupConnection = (callback0_t)1;
  callback0_t   _onUserInitiatedReboot = NULL;
  callback0_t   _onConfigChange = NULL;

  bool isEnteringState() { return _state != _prevState; }
  void setStateEntered() { _prevState = _state; }

} BlynkEdgent;

void Edgent::provisionCb() {
  BlynkEdgent.provisioned();
}

#include <BlynkEdgentConsole.h>

BLYNK_WRITE(InternalPinDBG) {
  BlynkEdgent.getConsole().runCommand(param.asStr());
}

#if !defined(BLYNK_CUSTOM_OTA_HANDLER)
BLYNK_WRITE(InternalPinOTA) {
  BlynkEdgent.startOTA(param.asStr());
}
#endif
