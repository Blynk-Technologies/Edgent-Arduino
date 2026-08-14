/**
 * @author     Volodymyr Shymanskyy
 * @copyright  Copyright (c) 2026 Volodymyr Shymanskyy
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NetMgrEsp32WiFi_h
#define NetMgrEsp32WiFi_h

#include <NetMgrUtils.h>
#include <WiFi.h>
#include <Preferences.h>
#include <list>

extern "C" {
  #include <esp_wifi.h>
}

#define NETMGR_WIFI_PREFS_NS        "nm_wifi"
#define NETMGR_WIFI_MAX_NETWORKS    16
#define NETMGR_WIFI_SCAN_ITVL_MIN   3
#define NETMGR_WIFI_SCAN_ITVL_MAX   (5*60)

#if defined(CONFIG_SOC_WIFI_SUPPORT_5G) && CONFIG_SOC_WIFI_SUPPORT_5G
    #define NETMGR_WIFI_SUPPORT_5G
    #define NETMGR_WIFI_SCAN_TIMEOUT_MS 30000
#else
    #define NETMGR_WIFI_SCAN_TIMEOUT_MS 15000
#endif
#define NETMGR_WIFI_CONN_TIMEOUT_MS 25000

class NetMgrEsp32WiFi
{
    class ApEntry {
    public:
        ApEntry(int8_t id)
            : id(id), rssi(0), security(WIFI_AUTH_MAX)
            , channel(0), hidden(false)
        {
            memset(bssid, 0, sizeof(bssid));
        }

        // Dynamic
        int8_t    id;
        int       rssi;
        wifi_auth_mode_t security;

        // Persistent
        String    ssid;
        String    psk;
        uint8_t   bssid[6];
        uint8_t   channel;
        bool      hidden;

        IPAddress localIP, gateway, subnet;
        IPAddress dns1, dns2;

    public:
        bool hasBSSID() const {
            return macIsValid(bssid);
        }

        size_t save(uint8_t* buff, size_t size) const {
            NetMgrBufferWriter tlv(buff, size);
            if (ssid.length())  { tlv.writeUInt8(FIELD_SSID);     tlv.write(ssid); }
            if (psk.length())   { tlv.writeUInt8(FIELD_PSK);      tlv.write(psk);  }
            if (hasBSSID())     { tlv.writeUInt8(FIELD_BSSID);    tlv.write(bssid, sizeof(bssid)); }
            if (channel)        { tlv.writeUInt8(FIELD_CHANNEL);  tlv.writeUInt8(channel); }
            if (hidden)         { tlv.writeUInt8(FIELD_HIDDEN);   }

            if (localIP)        { tlv.writeUInt8(FIELD_LOCAL_IP); tlv.write(localIP); }
            if (gateway)        { tlv.writeUInt8(FIELD_GATEWAY);  tlv.write(gateway); }
            if (subnet)         { tlv.writeUInt8(FIELD_MASK);     tlv.write(subnet);  }
            if (dns1)           { tlv.writeUInt8(FIELD_DNS);      tlv.write(dns1); }
            if (dns2)           { tlv.writeUInt8(FIELD_DNS);      tlv.write(dns2); }

            return tlv.getOffset();
        }

        bool load(const uint8_t* buff, size_t size) {
            NetMgrBufferReader tlv(buff, size);
            uint8_t tag;
            int dnsIdx = 0;
            while (tlv.readUInt8(tag)) {
                switch (tag) {
                case FIELD_SSID:      tlv.read(ssid);     break;
                case FIELD_PSK:       tlv.read(psk);      break;
                case FIELD_BSSID:     tlv.read(bssid, sizeof(bssid)); break;
                case FIELD_CHANNEL:   tlv.readUInt8(channel);         break;
                case FIELD_HIDDEN:    hidden = true;      break;

                case FIELD_LOCAL_IP:  tlv.read(localIP);  break;
                case FIELD_GATEWAY:   tlv.read(gateway);  break;
                case FIELD_MASK:      tlv.read(subnet);   break;
                case FIELD_DNS: {
                    if (dnsIdx++ == 0) {
                        tlv.read(dns1);
                    } else {
                        tlv.read(dns2);
                    }
                } break;
                default:
                    return 0;
                }
            }
            return tlv.getOffset() == size;
        }

    private:
        // WARNING: these constants should not be modified,
        //          only new values can be added
        enum FieldTag : uint8_t {
            FIELD_SSID      = 1,    // length: 0..255
            FIELD_PSK       = 2,    // length: 0..255
            FIELD_BSSID     = 3,    // length: 6
            FIELD_CHANNEL   = 4,    // length: 1
            FIELD_HIDDEN    = 5,    // length: 0

            FIELD_LOCAL_IP  = 10,   // length: 4
            FIELD_GATEWAY   = 11,   // length: 4
            FIELD_MASK      = 12,   // length: 4
            FIELD_DNS       = 13,   // length: 4
        };
    };

    typedef std::list<ApEntry> ApList;

public:
    enum State {
        NETMGR_IDLE,
        NETMGR_START_DOWN,
        NETMGR_WAIT_SCAN,
        NETMGR_START_SCANNING,
        NETMGR_SCANNING,
        NETMGR_START_CONNECTING,
        NETMGR_CONNECTING,
        NETMGR_CONNECTED,

        NETMGR_STATE_QTY
    } _state;

    enum Error {
        NETMGR_ERR_NONE,
        NETMGR_ERR_NETWORK_NOT_FOUND,
        NETMGR_ERR_AUTH_FAILED,
        NETMGR_ERR_IP_NOT_ASSIGNED,
        NETMGR_ERR_UNKNOWN,

        NETMGR_ERROR_QTY
    } _error;

    const char* getStateStr(State s) {
      static const char* StateStr[NETMGR_STATE_QTY] = {
        "IDLE",
        "START_DOWN",
        "WAIT_SCAN",
        "START_SCANNING",
        "SCANNING",
        "START_CONNECTING",
        "CONNECTING",
        "CONNECTED"
      };
      return StateStr[s];
    }

    const char* getErrorStr(Error e) {
      static const char* ErrorStr[NETMGR_ERROR_QTY] = {
        NULL,
        "NETWORK_NOT_FOUND",
        "AUTH_FAILED",
        "IP_NOT_ASSIGNED",
        "UNKNOWN"
      };
      return ErrorStr[e];
    }

public:
    NetMgrEsp32WiFi() {}

    ~NetMgrEsp32WiFi() {
        this->off();
        WiFi.removeEvent(_staDisconnectEvent);
    }

    void begin() {
        WiFi.persistent(false);
        WiFi.setAutoReconnect(false);
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0))
        // Allow connecting to legacy networks
        WiFi.setMinSecurity(WIFI_AUTH_WEP);
#endif

        _staDisconnectEvent = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
            const int reason = info.wifi_sta_disconnected.reason;
            switch (reason) {
            case WIFI_REASON_AUTH_EXPIRE:
            case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            case WIFI_REASON_AUTH_FAIL:
            case WIFI_REASON_HANDSHAKE_TIMEOUT:
            case WIFI_REASON_MIC_FAILURE:
                LOG_D("Auth failed (%d)", reason);
                _error = NETMGR_ERR_AUTH_FAILED;
                break;
            case WIFI_REASON_NO_AP_FOUND:
                LOG_D("Network not found");
                _error = NETMGR_ERR_NETWORK_NOT_FOUND;
                break;
            default:
                LOG_D("Disconnected (%d)", reason);
                _error = NETMGR_ERR_UNKNOWN;
            }
            if (_state == NETMGR_CONNECTED) {
                resetScanInterval();
                setState(NETMGR_START_SCANNING);
            }
        }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

        WiFi.setHostname(_hostname.c_str());

        resetScanInterval();
        loadNetworks();
    }

    void startConfig() {
        WiFi.enableSTA(true);
        // Start scanning immediately to improve responsiveness and UX,
        // esp. with 5GHz devices
        WiFi.scanNetworks(true, false, false);
    }

    bool isConfigured() {
        return _apList.size() > 0;
    }

    void on() {
        if (_state == NETMGR_IDLE) {
          _error = NETMGR_ERR_NONE;

          resetScanInterval();

          if (_lruId >= 0) {
              for (auto it = _apList.begin(); it != _apList.end(); ++it) {
                  ApEntry& ap = *it;
                  if (ap.id == _lruId && macIsValid(ap.bssid)) {
                      ApEntry foundAP = ap; // make a copy
                      _foundList.push_back(foundAP);
                      _foundBest = &_foundList.back();
                      setState(NETMGR_START_CONNECTING);
                      return;
                  }
              }
          }

          setState(NETMGR_START_SCANNING);
        }
    }

    void off() {
        setState(NETMGR_START_DOWN);
    }

    void setHostname(const String& hostname) {
        _hostname = hostname;
        _hostname.replace(" ", "-");
    }

    bool isHardwareAvailable() {
        return true;
    }

    bool supportsScan() { return true; }
    bool supports5GHz() {
#if defined(NETMGR_WIFI_SUPPORT_5G)
        return true;
#else
        return false;
#endif
    }
    bool supportsStaticIP() { return true; }

    bool isConnected() {
        return (WiFi.status() == WL_CONNECTED &&
                ipIsValid(WiFi.localIP()));
    }

    Error getError() const {
        return _error;
    }

    const char* getErrorStr() {
        return getErrorStr(_error);
    }

    const char* getStateStr() {
        return getStateStr(_state);
    }

    String getMacAddress() {
        byte mac[6];
        memset(mac, 0, sizeof(mac));
        WiFi.macAddress(mac);
        return macToString(mac);
    }

    String getLocalIP() {
        return ipToString(WiFi.localIP());
    }

    String getStatus() {
      if (_state == NETMGR_IDLE) {
        return "off";
      } else if (WiFi.status() == WL_CONNECTED) {
        if (WiFi.localIP()) {
          return "ready";
        } else {
          return "up";
        }
      } else {
        return "down";
      }
      return "unknown";
    }

    String getNetworkSSID() {
        return WiFi.SSID();
    }

    String getNetworkBSSID() {
        return WiFi.BSSIDstr();
    }

    int getRSSI() {
        return WiFi.RSSI();
    }

    String getApBSSID() {
        return WiFi.softAPmacAddress();
    }

    int scanNetworks() {
        const int res = getScanResult();
        if (res >= 0) {
            processScanResults(res, true);
        } else if (res != WIFI_SCAN_RUNNING) {
            // Update the list in background
            WiFi.scanNetworks(true, false, false);
        }
        if (!_scanList.size()) {
            delay(100);
            // Wait for any ongoing scan to complete
            while (getScanResult() == WIFI_SCAN_RUNNING){
                delay(100);
            }
            processScanResults(getScanResult(), true);
        }
        return _scanList.size();
    }

    void scanDelete() {
        // Already deleted in processScanResults
    }

    bool scanGetResult(int i, String& ssid, String& sec,
                       int& rssi, String& bssid, int& chan)
    {
        if (i < 0 || i >= _scanList.size()) {
            return false;
        }
        auto it = std::next(_scanList.begin(), i);
        const ApEntry& res = *it;

        ssid = res.ssid;
        rssi = res.rssi;
        chan = res.channel;
        sec = wifiSecToStr(res.security);
        bssid = macToString(res.bssid);
        return true;
    }

    bool addNetwork(const String& ssid) {
        return addNetwork(ssid, "");
    }

    bool addNetwork(const String& ssid, const String& psk) {
        LOG_I("Add DHCP network config SSID=%s PSK_len=%d", ssid.c_str(), psk.length());
        return addNetwork(ssid, psk, IPAddress(), IPAddress(), IPAddress(), IPAddress(), IPAddress());
    }

    bool addNetwork(const String& ssid,
                    const String& psk,
                    const String& ipStr,
                    const String& gwStr,
                    const String& maskStr,
                    const String& dns1Str,
                    const String& dns2Str)
    {
        LOG_I("Add static network config SSID=%s PSK_len=%d IP=%s GW=%s MASK=%s DNS1=%s DNS2=%s",
            ssid.c_str(), psk.length(), ipStr.c_str(), gwStr.c_str(),
            maskStr.c_str(), dns1Str.c_str(), dns2Str.c_str());

        auto toIP = [](const String& s) -> IPAddress {
            IPAddress ip;
            if (s.length() && ip.fromString(s)) {
                return ip;
            }
            return INADDR_NONE;
        };

        IPAddress ip   = toIP(ipStr);
        IPAddress gw   = toIP(gwStr);
        IPAddress mask = toIP(maskStr);
        IPAddress dns1 = toIP(dns1Str);
        IPAddress dns2 = toIP(dns2Str);

        return addNetwork(ssid, psk, ip, gw, mask, dns1, dns2);
    }

    bool addNetwork(const String& ssid,
                    const String& psk,
                    const IPAddress& IP,
                    const IPAddress& GW,
                    const IPAddress& Mask,
                    const IPAddress& DNS1,
                    const IPAddress& DNS2)
    {
        if (!ssid.length() || ssid.length() > 31) {
            LOG_E("No ssid or ssid too long");
            return false;
        }

        if (psk.length()) {
            if (psk.length() > 64) {
                LOG_E("Passphrase too long");
                return false;
            } else if (psk.length() < 8) {
                LOG_E("Passphrase too short");
                return false;
            }
        }

        // Replace existing network with the same SSID
        for (auto it = _apList.begin(); it != _apList.end(); ++it) {
            ApEntry& ap = *it;
            if (ap.ssid == ssid) {
                int existingID = ap.id;

                ApEntry newAP(existingID);
                newAP.ssid = ssid;
                newAP.psk = psk;
                newAP.localIP = IP;
                newAP.gateway = GW;
                newAP.subnet = Mask;
                newAP.dns1 = DNS1;
                newAP.dns2 = DNS2;
                
                if (saveNetwork(newAP)) {
                    *it = newAP;
                    //it = _apList.erase(it);
                    //_apList.insert(it, newAP);
                    setState(NETMGR_START_SCANNING);
                    return true;
                }
                return false;
            }
        }

        // Create new entry (possibly overwriting the oldest one)
        ApEntry newAP(_nextId);
        newAP.ssid = ssid;
        newAP.psk = psk;
        newAP.localIP = IP;
        newAP.gateway = GW;
        newAP.subnet = Mask;
        newAP.dns1 = DNS1;
        newAP.dns2 = DNS2;

        // Save incomplete AP info immediately
        Preferences prefs;
        if (prefs.begin(NETMGR_WIFI_PREFS_NS)) {
          if (saveNetwork(prefs, newAP)) {
            // Add item to the list
            _apList.push_back(newAP);

            // Update nextID
            _nextId = (_nextId+1) % NETMGR_WIFI_MAX_NETWORKS;
            prefs.putUShort("apID", _nextId);

            // Apply max APs count
            while (_apList.size() > NETMGR_WIFI_MAX_NETWORKS) {
                _apList.pop_front();
            }

            if (_apList.size() == 1) {
                ApEntry foundAP = _apList.front(); // make a copy
                _foundList.push_back(foundAP);
                _foundBest = &_foundList.back();
                setState(NETMGR_START_CONNECTING);
                return true;
            } else {
                setState(NETMGR_START_SCANNING);
                return true;
            }
          }
        }

        return false;
    }

    void clearNetworks() {
        _nextId = 0;
        _foundBest = NULL;
        _foundList.clear();
        _scanList.clear();
        _apList.clear();
        eraseNetworks();
    }

    void run() {
        switch (_state) {
        case NETMGR_IDLE: {
            // Idle
        } break;
        case NETMGR_START_DOWN: {
            WiFi.mode(WIFI_OFF);
            setState(NETMGR_IDLE);
        } break;
        case NETMGR_WAIT_SCAN: {
            if (millis() - _lastScanTime > _scanInterval*1000) {
                setState(NETMGR_START_SCANNING);
            }
        } break;
        case NETMGR_START_SCANNING: {
            if (!_apList.size()) {
                setState(NETMGR_IDLE);
                return;
            }
            _lastScanTime = millis();
            //                          async|hidden|passive
            int res = WiFi.scanNetworks(true, false, false);
            if (res == WIFI_SCAN_RUNNING || res >= 0) {
                setState(NETMGR_SCANNING);
            } else {
                LOG_D("Scanning not started");
                resetScanInterval();
                setState(NETMGR_WAIT_SCAN);
            }
        } break;
        case NETMGR_SCANNING: {
            const int scanResult = getScanResult();
            const bool timeout = (millis() - _lastStateTime > NETMGR_WIFI_SCAN_TIMEOUT_MS);
            if (scanResult >= 0) {
                processScanResults(scanResult);

                LOG_I("Found APs: %d, known: %d, time: %lu ms",
                      scanResult, _foundList.size(),
                      millis() - _lastScanTime);

                if (isConnected()) {
                    setState(NETMGR_CONNECTED);
                } else if ((_foundBest = findBestNetwork())) {
                    setState(NETMGR_START_CONNECTING);
                } else {
                    increaseScanInterval();
                    setState(NETMGR_WAIT_SCAN);
                }
            } else if (timeout || scanResult != WIFI_SCAN_RUNNING) {
                if (timeout) {
                    LOG_W("Scan timeout");
                } else {
                    LOG_W("Scan error: %d", scanResult);
                }
                WiFi.scanDelete();
                WiFi.disconnect(true, true);
                setState(NETMGR_START_SCANNING);
            }
        } break;
        case NETMGR_START_CONNECTING: {
            if (!_foundBest) {
                setState(NETMGR_WAIT_SCAN);
                return;
            }
            bool isDHCP = !_foundBest->localIP;
            if (isDHCP) {
                LOG_I("WiFi connect: %s (%s) ch:%d DHCP",
                    _foundBest->ssid.c_str(),
                    macToString(_foundBest->bssid).c_str(),
                    _foundBest->channel);
            } else {
                LOG_I("WiFi connect: %s (%s) ch:%d STATIC IP:%s MASK:%s GW:%s DNS1:%s DNS2:%s",
                    _foundBest->ssid.c_str(),
                    macToString(_foundBest->bssid).c_str(),
                    _foundBest->channel,
                    _foundBest->localIP.toString().c_str(),
                    _foundBest->subnet.toString().c_str(),
                    _foundBest->gateway.toString().c_str(),
                    _foundBest->dns1.toString().c_str(),
                    _foundBest->dns2.toString().c_str());
            }
            _error = NETMGR_ERR_NONE;
            WiFi.disconnect();

            WiFi.setHostname(_hostname.c_str());
            if (_foundBest->localIP) {
                // If dns2 is zero, WiFi.config will treat it as 0.0.0.0 (ok).
                WiFi.config(_foundBest->localIP, _foundBest->gateway, _foundBest->subnet, _foundBest->dns1, _foundBest->dns2);
            } else {
                WiFi.config(IPAddress(), IPAddress(), IPAddress()); // ensure DHCP
            }
            if (WiFi.begin(_foundBest->ssid.c_str(),
                           _foundBest->psk.c_str(),
                           _foundBest->channel,
                           macIsValid(_foundBest->bssid) ? _foundBest->bssid : NULL))
            {
                setState(NETMGR_CONNECTING);
            } else {
                LOG_W("Cannot start connection");
                WiFi.disconnect(true);
                setState(NETMGR_START_SCANNING);
            }
        } break;
        case NETMGR_CONNECTING: {
            if (millis() - _lastCheckTime > 10) {
                _lastCheckTime = millis();
                const int status = WiFi.status();
                const bool timeout = (millis() - _lastStateTime > NETMGR_WIFI_CONN_TIMEOUT_MS);
                if (status == WL_CONNECTED) {
                    LOG_I("WiFi connected "
                        "(BSSID: %s, IP: %s, MASK: %s, GW: %s, DNS1: %s, DNS2: %s, RSSI: %d)",
                        WiFi.BSSIDstr().c_str(),
                        WiFi.localIP().toString().c_str(),
                        WiFi.subnetMask().toString().c_str(),
                        WiFi.gatewayIP().toString().c_str(),
                        WiFi.dnsIP(0).toString().c_str(),
                        WiFi.dnsIP(1).toString().c_str(),
                        WiFi.RSSI());

                    Preferences prefs;
                    if (prefs.begin(NETMGR_WIFI_PREFS_NS)) {
                        // Check if AP info needs to be updated
                        for (auto it = _apList.begin(); it != _apList.end(); ++it) {
                            ApEntry& ap = *it;
                            if (!ap.hasBSSID() && ap.id == _foundBest->id) {
                                memcpy(ap.bssid, _foundBest->bssid, sizeof(ap.bssid));
                                ap.channel = _foundBest->channel;
                                ap.security = _foundBest->security;

                                // Save the updated info
                                saveNetwork(prefs, ap);
                                break;
                            }
                        }

                        // Save LRU ID
                        if (_lruId != _foundBest->id) {
                            _lruId = _foundBest->id;
                            prefs.putShort("lru", _lruId);
                            LOG_D("LRU set to %d", _lruId);
                        }
                    }

                    setState(NETMGR_CONNECTED);
                } else if (status == WL_NO_SSID_AVAIL) {
                    LOG_E("Connecting failed: AP not found");
                    WiFi.disconnect(true);
                    setState(NETMGR_START_SCANNING);
                } else if (status == WL_CONNECT_FAILED || timeout) {
                    if (_error == NETMGR_ERR_AUTH_FAILED) {
                        LOG_E("Connecting failed: Authentication failed (wrong password?)");
                    } else if (timeout) {
                        LOG_E("Connecting failed: timeout");
                    } else {
                        LOG_E("Connecting failed");
                    }
                    WiFi.disconnect(true);
                    _foundBest->rssi = 0; // exclude from next connection
                    if ((_foundBest = findBestNetwork())) {
                        setState(NETMGR_START_CONNECTING);
                    } else {
                        increaseScanInterval();
                        setState(NETMGR_WAIT_SCAN);
                    }
                }
            }
        } break;
        case NETMGR_CONNECTED: {
            if (WiFi.status() != WL_CONNECTED) {
                LOG_W("Lost network");
                resetScanInterval();
                setState(NETMGR_START_SCANNING);
            }
        } break;
        default: {
            setState(NETMGR_IDLE);
        }
        }
    }

public:

    static
    const char* wifiSecToStr(int sec) {
      switch (sec) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA+WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 ENT";
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2+WPA3";
        case WIFI_AUTH_WAPI_PSK:        return "WAPI";
#endif
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0))
        case WIFI_AUTH_OWE:             return "OWE";
        case WIFI_AUTH_WPA_ENTERPRISE:  return "WPA ENT";
        case WIFI_AUTH_WPA3_ENTERPRISE: return "WPA3 ENT";
        case WIFI_AUTH_WPA2_WPA3_ENTERPRISE: return "WPA2+WPA3 ENT";
#endif
        default:                        return "unknown";
      }
    }
private:

    void setState(State s) {
        if (_state != s) {
            LOG_D("%s -> %s", getStateStr(_state), getStateStr(s));
        }
        _state = s;
        _lastCheckTime = _lastStateTime = millis();
    }

    ApEntry* findBestNetwork() {
        ApEntry* result = NULL;
        int bestRSSI = INT_MIN;
        for (auto it = _foundList.begin(); it != _foundList.end(); ++it) {
            ApEntry& entry = *it;
            if (entry.rssi < 0 && bestRSSI < entry.rssi) {
                bestRSSI = entry.rssi;
                result = &entry;
            }
        }
        return result;
    }

    int getScanResult() {
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0))
        return WiFi.scanComplete();
#else
        // WiFi.scanComplete() should be enough but there is a bug
        // in Arduino Core that causes async scan to fail with a timeout
        if  (WiFiGenericClass::getStatusBits() & WIFI_SCAN_DONE_BIT) {
            uint16_t scanCount = 0;
            esp_wifi_scan_get_ap_num(&scanCount);
            return scanCount;
        }
        return WIFI_SCAN_RUNNING;
#endif
    }

    void processScanResults(const int scanResult, bool fullList = false) {
        _foundList.clear();
        _scanList.clear();
        _foundBest = NULL;

        for (int i = 0; i < scanResult; ++i) {
            String ssid_scan;
            int32_t rssi_scan;
            uint8_t sec_scan;
            uint8_t* bssid_scan;
            int32_t chan_scan;

            WiFi.getNetworkInfo(i, ssid_scan, sec_scan, rssi_scan, bssid_scan, chan_scan);

            if (fullList) {
                ApEntry scanEntry(0);
                scanEntry.ssid = ssid_scan;
                scanEntry.rssi = rssi_scan;
                scanEntry.security = static_cast<wifi_auth_mode_t>(sec_scan);
                memcpy(scanEntry.bssid, bssid_scan, sizeof(scanEntry.bssid));
                scanEntry.channel = chan_scan;
                _scanList.push_back(scanEntry);
            }

            bool known = false;
            for (auto it = _apList.rbegin(); it != _apList.rend(); ++it) {
                ApEntry& entry = *it;

                if ((ssid_scan == entry.ssid) &&
                    ((sec_scan == WIFI_AUTH_OPEN && !entry.psk.length()) ||
                     (sec_scan != WIFI_AUTH_OPEN && entry.psk.length()))
                ) {
                    ApEntry foundAP = entry; // make a copy

                    memcpy(foundAP.bssid, bssid_scan, sizeof(foundAP.bssid));
                    foundAP.channel = chan_scan;
                    foundAP.security = static_cast<wifi_auth_mode_t>(sec_scan);
                    foundAP.rssi = rssi_scan;

                    _foundList.push_back(foundAP);

                    known = true;
                    break;
                }
            }

            (void)known;

            LOG_D(" %s  %d: [%d][%s] %s (%d) %c", known?">":" ", i, chan_scan,
                  macToString(bssid_scan).c_str(), ssid_scan.c_str(),
                  rssi_scan, (sec_scan == WIFI_AUTH_OPEN) ? ' ' : '*'
                  );
        }
        WiFi.scanDelete();
    }

    bool loadNetwork(Preferences& prefs, int id) {
        char key[8];
        snprintf(key, sizeof(key), "ap%02d", id);
        if (prefs.isKey(key)) {
            uint8_t buf[128];
            size_t len = prefs.getBytes(key, buf, sizeof(buf));
            ApEntry ap(id);
            if (len && ap.load(buf, len)) {
                LOG_D("Loaded %s, ssid: %s, bssid: %s", key, ap.ssid.c_str(), macToString(ap.bssid).c_str());
                _apList.push_back(ap);
                return true;
            } else {
                LOG_W("Cannot load %s (%d bytes)", key, len);
            }
        }
        return false;
    }

    void loadNetworks() {
        Preferences prefs;
        if (prefs.begin(NETMGR_WIFI_PREFS_NS, true)) { // read-only
            if (prefs.isKey("apID")) {
                _nextId = prefs.getUShort("apID", 0);
                // Ensure next ID is in range
                _nextId = min(_nextId, NETMGR_WIFI_MAX_NETWORKS-1);
            }

            // Load networks starting from the oldest one
            for (int id = _nextId; id < NETMGR_WIFI_MAX_NETWORKS; id++) {
                loadNetwork(prefs, id);
            }
            for (int id = 0; id < _nextId; id++) {
                loadNetwork(prefs, id);
            }

            if (prefs.isKey("lru")) {
                _lruId = prefs.getShort("lru", -1);
                LOG_D("LRU ID: %d", _lruId);
            }
        }
    }

    bool saveNetwork(Preferences& prefs, const ApEntry& ap) {
        char key[8];
        snprintf(key, sizeof(key), "ap%02d", ap.id);
        uint8_t buf[256];
        size_t len = ap.save(buf, sizeof(buf));
        if (len && prefs.putBytes(key, buf, len) == len) {
            LOG_D("Saved SSID: %s, ID: %d (%d bytes)", ap.ssid.c_str(), ap.id, len);
            return true;
        }
        LOG_E("Cannot save SSID: %s, ID: %d", ap.ssid.c_str(), ap.id);
        return false;
    }

    bool saveNetwork(const ApEntry& ap) {
        Preferences prefs;
        if (prefs.begin(NETMGR_WIFI_PREFS_NS)) {
            return saveNetwork(prefs, ap);
        }
        return false;
    }

    void eraseNetworks() {
        Preferences prefs;
        if (prefs.begin(NETMGR_WIFI_PREFS_NS)) {
            prefs.clear();
        }
    }

    void resetScanInterval() {
        _scanInterval = NETMGR_WIFI_SCAN_ITVL_MIN;
    }

    void increaseScanInterval() {
        if (_scanInterval < NETMGR_WIFI_SCAN_ITVL_MIN) {
            _scanInterval = NETMGR_WIFI_SCAN_ITVL_MIN;
        } else if (_scanInterval < NETMGR_WIFI_SCAN_ITVL_MAX) {
            _scanInterval = _scanInterval * 1.7;
            if (_scanInterval > NETMGR_WIFI_SCAN_ITVL_MAX) {
                _scanInterval = NETMGR_WIFI_SCAN_ITVL_MAX;
            }
            LOG_D("Scan interval: %lu", _scanInterval);
        }
    }

private:
    uint32_t          _lastScanTime = 0;
    uint32_t          _lastStateTime = 0;
    uint32_t          _lastCheckTime = 0;
    uint32_t          _scanInterval;
    int               _nextId = 0;
    int               _lruId  = -1;
    ApList            _apList;
    ApList            _foundList;
    ApList            _scanList;
    ApEntry*          _foundBest = NULL;
    WiFiEventId_t     _staDisconnectEvent;
    String            _hostname;
};

#endif /* NetMgrEsp32WiFi_h */
