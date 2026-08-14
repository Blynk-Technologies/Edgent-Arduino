/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "BlynkInject.h"

#if defined(CONFIG_USE_INJECT_WIFIAP)

#include "NetMgr.h"
#include "BlynkSysUtils.h"
#include <updater/BlynkUpdater.h>
#include <ArduinoJson.h>

#if defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  typedef ESP8266WebServer WebServer;
#endif
#include <DNSServer.h>

static const char configForm[] PROGMEM = R"html(
<!DOCTYPE HTML>
<html><head>
  <title>WiFi setup</title>
  <style>
  body {
    background-color: #fcfcfc;
    box-sizing: border-box;
  }
  body, input {
    font-family: Roboto, sans-serif;
    font-weight: 400;
    font-size: 16px;
  }
  .centered {
    position: fixed;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);

    padding: 20px;
    background-color: #ccc;
    border-radius: 4px;
  }
  td { padding:0 0 0 5px; }
  label { white-space:nowrap; }
  input { width: 20em; }
  input[name="port"] { width: 5em; }
  input[type="submit"], img { margin: auto; display: block; width: 30%; }
  </style>
</head><body>
<div class="centered">
  <form method="get" action="config">
    <input type="hidden" name="if" value="wifi">
    <table>
    <tr><td><label for="ssid">WiFi network:</label></td>  <td><input type="text" name="ssid" maxlength=64 required="required"></td></tr>
    <tr><td><label for="pass">WiFi password:</label></td> <td><input type="text" name="pass" maxlength=64></td></tr>
    <tr><td><label for="blynk">Auth token:</label></td>   <td><input type="text" name="blynk" placeholder="a0b1c2d..." pattern="[-_a-zA-Z0-9]{32}" minlength="32" maxlength="32" required="required"></td></tr>
    <tr><td><label for="host">Server:</label></td>        <td><input type="text" name="host" value="" maxlength=64 required="required"></td></tr>
    </table><br/>
    <input type="submit" value="Apply">
  </form>
</div>
</body></html>
)html";

static const char serverUpdateForm[] PROGMEM = R"html(
<html><body>
  <form method='POST' action='' enctype='multipart/form-data'>
    <input type='file' name='update' accept='.bin'>
    <input type='submit' value='Update'>
  </form>
</body></html>
)html";

#define WIFI_AP_IP                    IPAddress(192, 168, 4, 1)
#define WIFI_AP_Subnet                IPAddress(255, 255, 255, 0)
#define CONFIG_AP_URL                 "blynk.setup"

WebServer     server(80);
DNSServer     dnsServer;
const byte    DNS_PORT = 53;


BlynkInject::BlynkInject()
{}

bool BlynkInject::isUserConfiguring() {
    return _user_started_configuring && WiFi.softAPgetStationNum();
}
bool BlynkInject::isAppDisconnected() {
    return _user_started_configuring && !WiFi.softAPgetStationNum();
}

static inline
void sendMsg(const char* str) {
    server.send(200, "application/json", str);
    //LOG_D("<< %s", str);
}

static inline
void sendMsg(const void* data, unsigned len) {
    //LOG_D("<< %s", data);
    server.setContentLength(len);
    server.send(200, "application/json", "");
    server.sendContent((const char*)data, len);
}

void BlynkInject::begin(String name, String vendor, String tmpl_id, String fw_type, String fw_ver)
{
    if (_started) return;
    _started = true;

    _name = name.substring(0, 31);
    _vendor = vendor;
    _tmpl_id = tmpl_id;
    _fw_type = fw_type;
    _fw_ver = fw_ver;
    _user_started_configuring = false;
    _last_error = ERROR_NONE;

    clearRuntimeConfig();

#ifdef NetMgr_WiFi
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    WiFi.softAPConfig(WIFI_AP_IP, WIFI_AP_IP, WIFI_AP_Subnet);
    WiFi.softAP(_name.c_str());
    delay(50);
#endif

    setupServer();

    LOG_I("Provisioning started");
}

void BlynkInject::end()
{
    server.stop();
    WiFi.enableAP(false);
    _started = false;
    _user_started_configuring = false;
    LOG_I("Provisioning finished");
}

void BlynkInject::reportStatus(InjectStatus status) {
    // Not supported
}

void BlynkInject::reportNetStatus() {
    // Not supported
}

void BlynkInject::reportFailure(InjectError error, const String& msg) {
    _last_error = error;
}

void BlynkInject::clearRuntimeConfig() {
    _config.intf = _config.ssid = _config.pass = _config.auth = "";
    _config.host = _config.ip = _config.mask = _config.gw = _config.dns = _config.dns2 = "";
    _config.forceSave = false;
}

void BlynkInject::run() {
    if (!_started) return;

    dnsServer.processNextRequest();
    server.handleClient();
}

void BlynkInject::setupServer() {

      // Set up DNS Server
      dnsServer.setTTL(300); // Time-to-live 300s
      dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure); // Return code for non-accessible domains
#ifdef WIFI_CAPTIVE_PORTAL_ENABLE
      dnsServer.start(DNS_PORT, "*", WiFi.softAPIP()); // Point all to our IP
      server.onNotFound([]() {
        server.send(200, "text/html", configForm);
      });
#else
      dnsServer.start(DNS_PORT, CONFIG_AP_URL, WiFi.softAPIP());
      LOG_I("WiFi AP config page: %s", CONFIG_AP_URL);
      server.onNotFound([]() {
        server.send(404, "text/plain", "Not found");
      });
#endif

      server.on("/update", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", serverUpdateForm);
      });
      server.on("/update", HTTP_POST, [this]() {
        server.sendHeader("Connection", "close");
        if (ArduinoUpdate.isRunning() && ArduinoUpdate.end()) {
          server.send(200, "text/plain", "OK");
          LOG_I("=== Update successfully completed. Rebooting.");
          delay(100);
          ArduinoUpdate.apply();
        } else {
          server.send(500, "text/plain", "FAIL");
        }
      }, [this]() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
          LOG_I("Update: %s", upload.filename.c_str());
          if (!ArduinoUpdate.begin()) {
            LOG_E("Not enough space to store the update");
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (ArduinoUpdate.isRunning()) {
            if (!ArduinoUpdate.write(upload.buf, upload.currentSize)) {
              LOG_E("OTA write failed");
              ArduinoUpdate.abort();
            }
          }
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
          ArduinoUpdate.abort();
        }
      });
      server.on("/", []() {
        server.send(200, "text/html", configForm);
      });
      server.on("/config", [this]() {
        LOG_I("Applying configuration...");
        _config.ssid = server.arg("ssid");
        String ssidManual = server.arg("ssidManual");
        if (ssidManual != "") {
          _config.ssid = ssidManual;
        }
        _config.intf = server.arg("if");
        _config.pass = server.arg("pass");
        _config.auth  = server.arg("blynk");
        _config.host  = server.arg("host");

        _config.ip   = server.arg("ip");
        _config.mask = server.arg("mask");
        _config.gw   = server.arg("gw");
        _config.dns  = server.arg("dns");
        _config.dns2 = server.arg("dns2");

        _config.forceSave  = server.arg("save").toInt();

        if (_config.auth.length() == 32 &&
            (_config.intf == "wifi" && _config.ssid.length())
        ) {
            sendMsg(R"json({"status":"ok","msg":"Trying to connect..."})json");

            if (provisionCb != nullptr) {
                provisionCb();
            }
        } else {
            LOG_W("Configuration invalid");
            server.send(500, "application/json", R"json({"status":"error","msg":"Configuration invalid"})json");
        }
      });
      server.on("/board_info.json", [this]() {
        LOG_I("Sending board info");

        // Configuring starts with board info request
        _user_started_configuring = true;

        JsonDocument writer;
        writer["vendor"  ] = _vendor;
        writer["tmpl_id" ] = _tmpl_id;
        writer["fw_type" ] = _fw_type;
        writer["fw_ver"  ] = _fw_ver;
        writer["name"    ] = _name;
        writer["uid"     ] = systemGetDeviceUID();
        writer["bssid"   ] = WiFi.softAPmacAddress();
        writer["last_error"] = (int)_last_error;
        JsonArray ifs = writer["ifs"].to<JsonArray>();
#ifdef NetMgr_WiFi
        if (NetMgrWiFi.isHardwareAvailable()) {
          JsonObject obj = ifs.add<JsonObject>();
          obj["name"  ] = "wifi";
          obj["mac"   ] = NetMgrWiFi.getMacAddress();
          obj["scan"  ] = NetMgrWiFi.supportsScan()?1:0;
          obj["5ghz"  ] = NetMgrWiFi.supports5GHz()?1:0;
          obj["static_ip"] = NetMgrWiFi.supportsStaticIP()?1:0;
        }
#endif
        String json;
        serializeJson(writer, json);
        sendMsg(json.c_str(), json.length());
      });
      server.on("/wifi_scan.json", []() {
        LOG_I("Scanning WiFi");

        int wifi_nets = NetMgrWiFi.scanNetworks();
        LOG_I("Found networks: %d", wifi_nets);
        wifi_nets = min(15, wifi_nets); // Use top 15 networks

        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < wifi_nets; i++) {
          String ssid, sec, bssid;
          int chan = -1, rssi = 0;
          if (!NetMgrWiFi.scanGetResult(i, ssid, sec, rssi, bssid, chan)) {
            continue;
          }
          // skip weak and hidden networks
          if (rssi >= -90 && ssid.length()) {

            JsonObject net = arr.add<JsonObject>();
            net["ssid"  ] = ssid;
            net["bssid" ] = bssid;
            net["rssi"  ] = rssi;
            net["sec"   ] = sec;
            net["ch"    ] = chan;
          }
        }
        NetMgrWiFi.scanDelete();
        String json;
        serializeJson(doc, json);
        sendMsg(json.c_str(), json.length());
      });
      server.on("/reset", []() {
#ifdef NetMgr_WiFi
        NetMgrWiFi.clearNetworks();
#endif
        sendMsg(R"json({"status":"ok","msg":"Configuration reset"})json");
      });
      server.on("/reboot", []() {
        systemReboot();
      });

      server.begin();

}

void BlynkInject::setProvisionCallback(provisionCb_t* cb) {
    provisionCb = cb;
}

#endif /* CONFIG_USE_INJECT_WIFIAP */
