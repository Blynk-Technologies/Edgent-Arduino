/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "BlynkInject.h"

#if defined(CONFIG_USE_INJECT_BLE)

  #include "NetMgr.h"
  #include "BlynkSysUtils.h"
  #include <ArduinoJson.h>


  #if defined(PARTICLE)
    #include "BlynkBleSparkBLE.h"
  #elif defined(NRF5)
    #include "BlynkBleNimBLE.h"
  #elif defined(ESP32)
    #include "BlynkBleNimBLE.h"
  //#include "BlynkBleBluedroid.h"
  #elif defined(SEEED_WIO_TERMINAL)
    #include "BlynkBleSeeedRpcBLE.h"
  #endif

static BlynkBLE _ble;

  #ifdef CONFIG_USE_OTA_BLE
    #include "BlynkBleOTA.h"
static BlynkBleOTA* _ota;
  #endif

static inline void sendMsg(const char* str) {
    _ble.write(str, strlen(str));
}

static inline void sendMsg(const void* data, unsigned len) {
    _ble.write(data, len);
}

BlynkInject::BlynkInject() {}

bool BlynkInject::isUserConfiguring() {
    return _user_started_configuring && _ble.isConnected();
}
bool BlynkInject::isAppDisconnected() {
    return _user_started_configuring && !_ble.isConnected();
}

void BlynkInject::begin(String name, String vendor, String tmpl_id, String fw_type, String fw_ver) {
    if (_started) return;
    _started = true;

    _name = name.substring(0, 29);
    _vendor = vendor;
    _tmpl_id = tmpl_id;
    _fw_type = fw_type;
    _fw_ver = fw_ver;
    _user_started_configuring = false;
    _last_status = STATUS_UNKNOWN;

    clearRuntimeConfig();

  #ifdef NetMgr_WiFi
    NetMgrWiFi.startConfig();
  #endif
  #ifdef NetMgr_Ethernet
    NetMgrEthernet.startConfig();
  #endif
  #ifdef NetMgr_Cellular
    NetMgrCellular.startConfig();
  #endif
  #ifdef CONFIG_USE_OTA_BLE
    _ble.onRawData = [](const uint8_t* data, size_t len) {
        if (_ota) { _ota->processRawPacket(data, len); }
    };
  #endif
    const String tmpl_name = systemGetTemplateName();
    _ble.begin(_name.c_str(), tmpl_name.c_str());
    LOG_I("Provisioning started");
}

void BlynkInject::end() {
    _ble.end();
    _started = false;
    _last_status = STATUS_UNKNOWN;
    _user_started_configuring = false;
    LOG_I("Provisioning finished");
}

void BlynkInject::reportStatus(InjectStatus status) {
    if (!_started || !status) {
        return;
    }
    if (_last_status == status) {
        return;
    }
    JsonDocument writer;
    writer["t"] = "status";
    switch (status) {
        case STATUS_UNKNOWN: writer["s"] = "unknown"; break;
        case STATUS_ERROR: writer["s"] = "error"; break;
        case STATUS_CONNECTING_NETWORK: writer["s"] = "connecting_net"; break;
        case STATUS_CONNECTING_CLOUD: writer["s"] = "connecting_cloud"; break;
        case STATUS_CONNECTED: writer["s"] = "connected"; break;
    }
    char buff[128];
    size_t len = serializeJson(writer, buff, sizeof(buff));
    sendMsg(buff, len);
    _last_status = status;
}

void BlynkInject::reportNetStatus() {
    if (!_started) return;
    char buff[256];
  #ifdef NetMgr_WiFi
    {
        JsonDocument writer;
        writer["t"] = "net_status";
        writer["if"] = "wifi";
        writer["state"] = NetMgrWiFi.getStateStr();
        writer["ssid"] = NetMgrWiFi.getNetworkSSID();
        writer["bssid"] = NetMgrWiFi.getNetworkBSSID();
        writer["ip"] = NetMgrWiFi.getLocalIP();
        writer["rssi"] = NetMgrWiFi.getRSSI();
        size_t len = serializeJson(writer, buff, sizeof(buff));
        sendMsg(buff, len);
    }
  #endif
  #ifdef NetMgr_Ethernet
    {
        JsonDocument writer;
        writer["t"] = "net_status";
        writer["if"] = "eth";
        writer["state"] = NetMgrEthernet.getStateStr();
        size_t len = serializeJson(writer, buff, sizeof(buff));
        sendMsg(buff, len);
    }
  #endif
  #ifdef NetMgr_Cellular
    {
        JsonDocument writer;
        writer["t"] = "net_status";
        writer["if"] = "cell";
        writer["state"] = NetMgrCellular.getStateStr();
        writer["operator"] = NetMgrCellular.getOperator();
        writer["rssi"] = NetMgrCellular.getRSSI();
        size_t len = serializeJson(writer, buff, sizeof(buff));
        sendMsg(buff, len);
    }
  #endif
}

void BlynkInject::sendError(const char* type, const char* reason, const String& msg) {
    if (!_started || !type) {
        return;
    }

    JsonDocument writer;
    writer["t"] = type;
    if (reason && reason[0]) {
        writer["reason"] = reason;
    }
    if (msg.length()) {
        writer["msg"] = msg;
    }
    char buff[256];
    size_t len = serializeJson(writer, buff, sizeof(buff));
    sendMsg(buff, len);
    _last_status = STATUS_ERROR;
}

void BlynkInject::reportFailure(InjectError error, const String& msg) {
    switch (error) {
        case ERROR_NONE: break;
        case ERROR_CONFIG: sendError("connect_fail", nullptr, msg); break;

        case ERROR_CLOUD_TIMEOUT: sendError("cloud_fail", "timeout", msg); break;
        case ERROR_CLOUD_TOKEN: sendError("cloud_fail", "auth_failed", msg); break;
        case ERROR_CLOUD_DNS_FAILED: sendError("cloud_fail", "dns_failed", msg); break;
        case ERROR_CLOUD_TLS_CERT_FAILED: sendError("cloud_fail", "invalid_certificate", msg); break;
        case ERROR_CLOUD_CAPTIVE_PORTAL: sendError("cloud_fail", "captive_portal", msg); break;
        case ERROR_CLOUD_GENERIC: sendError("cloud_fail", "generic", msg); break;

        case ERROR_NETWORK_TIMEOUT: sendError("net_fail", "timeout", msg); break;
        case ERROR_NETWORK_NOT_FOUND: sendError("net_fail", "not_found", msg); break;
        case ERROR_NETWORK_NO_CABLE: sendError("net_fail", "no_cable", msg); break;
        case ERROR_NETWORK_AUTH_FAIL: sendError("net_fail", "invalid_credentials", msg); break;
        case ERROR_NETWORK_NO_ADDRESS: sendError("net_fail", "no_ip_assigned", msg); break;
        case ERROR_NETWORK_GENERIC: sendError("net_fail", "generic", msg); break;

        case ERROR_SIMCARD_MISSING: sendError("net_fail", "sim_missing", msg); break;
        case ERROR_SIMCARD_LOCKED: sendError("net_fail", "sim_locked", msg); break;
        case ERROR_SIMCARD_WRONG_PIN: sendError("net_fail", "sim_wrong_pin", msg); break;
    }
}

void BlynkInject::clearRuntimeConfig() {
    _config.intf = _config.ssid = _config.pass = _config.auth = "";
    _config.host = _config.ip = _config.mask = _config.gw = _config.dns = _config.dns2 = "";
    _config.forceSave = false;
}

void BlynkInject::run() {
    if (!_started) return;

    parseMessage();
}

void BlynkInject::parseMessage() {
    if (!_ble.available()) return;

    String cmd = _ble.read();
    JsonDocument json;
    DeserializationError error = deserializeJson(json, cmd);
    if (error || !json.is<JsonObject>()) {
        sendMsg(R"json({"t":"error","msg":"wrong format"})json");
        return;
    }

    // Process our received message. Get type first.
    String t = json["t"];

    if (t == "set") {
        bool foundInvalid = false;
        JsonObject obj = json.as<JsonObject>();
        for (JsonPair item : obj) {
            const JsonString& key = item.key();
            if (key == "t") { /* skip */
            } else if (key == "if") {
                _config.intf = item.value().as<String>();
            } else if (key == "ssid") {
                _config.ssid = item.value().as<String>();
            } else if (key == "pass") {
                _config.pass = item.value().as<String>();
            } else if (key == "blynk") {
                _config.auth = item.value().as<String>();
            } else if (key == "host") {
                _config.host = item.value().as<String>();
            } else if (key == "port") { /* ignored */
            } else if (key == "ip") {
                _config.ip = item.value().as<String>();
            } else if (key == "mask") {
                _config.mask = item.value().as<String>();
            } else if (key == "gw") {
                _config.gw = item.value().as<String>();
            } else if (key == "dns") {
                _config.dns = item.value().as<String>();
            } else if (key == "dns2") {
                _config.dns2 = item.value().as<String>();
            } else if (key == "save") {
                _config.forceSave = item.value().as<bool>();
            } else {
                foundInvalid = true;
            }
        }
        if (!foundInvalid) {
            sendMsg(R"json({"t":"set_ok"})json");
        } else {
            sendMsg(R"json({"t":"set_fail"})json");
        }
    } else if (t == "connect") {
        // TODO: per-interface validation on NetMgg side?
        if (_config.auth.length() == 32 &&
            ((_config.intf == "wifi" && _config.ssid.length()) ||
             (_config.intf == "cell") ||
             (_config.intf == "eth"))) {
            sendMsg(R"json({"t":"connecting"})json");

            if (provisionCb != nullptr) {
                provisionCb();
            }
        } else {
            LOG_W("Configuration invalid");
            reportFailure(ERROR_CONFIG, "configuration invalid");
        }
    } else if (t == "info") {
        LOG_I("Sending board info");

        // Configuring starts with board info request
        _user_started_configuring = true;

        JsonDocument writer;
        writer["t"] = "info";
        writer["vendor"] = _vendor;
        writer["tmpl_id"] = _tmpl_id;
        writer["fw_type"] = _fw_type;
        writer["fw_ver"] = _fw_ver;
        writer["name"] = _name;
        writer["uid"] = systemGetDeviceUID();

        JsonArray caps = writer["caps"].to<JsonArray>();
        caps.add("live_status");

        char buff[256];
        size_t len = serializeJson(writer, buff, sizeof(buff));
        sendMsg(buff, len);
    } else if (t == "ifs") {
        LOG_I("Sending interface info");

        sendMsg(R"json({"t":"ifs_start"})json");
        char buff[256];
  #ifdef NetMgr_WiFi
        if (NetMgrWiFi.isHardwareAvailable()) {
            JsonDocument writer;
            writer["t"] = "if";
            writer["name"] = "wifi";
            writer["mac"] = NetMgrWiFi.getMacAddress();
            writer["scan"] = NetMgrWiFi.supportsScan() ? 1 : 0;
            writer["5ghz"] = NetMgrWiFi.supports5GHz() ? 1 : 0;
            writer["static_ip"] = NetMgrWiFi.supportsStaticIP() ? 1 : 0;
            size_t len = serializeJson(writer, buff, sizeof(buff));
            sendMsg(buff, len);
        }
  #endif
  #ifdef NetMgr_Cellular
        if (NetMgrCellular.isHardwareAvailable()) {
            JsonDocument writer;
            writer["t"] = "if";
            writer["name"] = "cell";
            writer["imei"] = NetMgrCellular.getIMEI();
            writer["imsi"] = NetMgrCellular.getIMSI();
            writer["iccid"] = NetMgrCellular.getICCID();
            writer["scan"] = NetMgrCellular.supportsScan() ? 1 : 0;
            writer["pin"] = NetMgrCellular.supportsSimPin() ? 1 : 0;
            writer["apn"] = NetMgrCellular.supportsAPN() ? 1 : 0;
            size_t len = serializeJson(writer, buff, sizeof(buff));
            sendMsg(buff, len);
        }
  #endif
  #ifdef NetMgr_Ethernet
        if (NetMgrEthernet.isHardwareAvailable()) {
            JsonDocument writer;
            writer["t"] = "if";
            writer["name"] = "eth";
            writer["mac"] = NetMgrEthernet.getMacAddress();
            writer["status"] = NetMgrEthernet.getStatus();
            if (NetMgrEthernet.isConnected()) {
                writer["ip"] = NetMgrEthernet.getLocalIP();
            }
            writer["static_ip"] = NetMgrEthernet.supportsStaticIP() ? 1 : 0;
            size_t len = serializeJson(writer, buff, sizeof(buff));
            sendMsg(buff, len);
        }
  #endif
        sendMsg(R"json({"t":"ifs_end"})json");
    } else if (t == "scan") {
  #ifdef NetMgr_WiFi
        LOG_I("Scanning WiFi");
        sendMsg(R"json({"t":"scan_start"})json");

        int wifi_nets = NetMgrWiFi.scanNetworks();
        LOG_I("Found networks: %d", wifi_nets);
        wifi_nets = min(30, wifi_nets);  // Use top 30 networks

        char buff[256];
        for (int i = 0; i < wifi_nets; i++) {
            String ssid, sec, bssid;
            int chan = -1, rssi = 0;
            if (!NetMgrWiFi.scanGetResult(i, ssid, sec, rssi, bssid, chan)) {
                continue;
            }
          // skip weak and hidden networks
            if (rssi >= -90 && ssid.length()) {

                JsonDocument writer;
                writer["t"] = "scan";
                writer["ssid"] = ssid;
                writer["bssid"] = bssid;
                writer["rssi"] = rssi;
                writer["sec"] = sec;
                writer["ch"] = chan;
                size_t len = serializeJson(writer, buff, sizeof(buff));
                sendMsg(buff, len);
            }
        }

        sendMsg(R"json({"t":"scan_end"})json");
        NetMgrWiFi.scanDelete();
  #else
        sendMsg(R"json({"t":"error","msg":"no wifi"})json");
  #endif
  #ifdef CONFIG_USE_OTA_BLE
    } else if (t.startsWith("ota_")) {
        if (!_ota) {
            _ota = new BlynkBleOTA(_ble);
        }
        JsonObject obj = json.as<JsonObject>();
        _ota->processCommand(obj);
  #endif
    } else if (t == "reset") {
  #ifdef NetMgr_WiFi
        NetMgrWiFi.clearNetworks();
  #endif
        sendMsg(R"json({"t":"reset_ok"})json");
    } else if (t == "reboot") {
        systemReboot();
    } else {
        sendMsg(R"json({"t":"error","msg":"invalid command"})json");
    }
}

void BlynkInject::setProvisionCallback(provisionCb_t* cb) {
    provisionCb = cb;
}

#endif /* CONFIG_USE_INJECT_BLE */
