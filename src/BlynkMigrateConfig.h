/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <Preferences.h>

bool migrateOldConfig(ConfigStore& cfg) {
    const uint8_t OLD_CONFIG_FLAG_VALID = 0x01;
    const uint8_t OLD_CONFIG_FLAG_STATIC_IP = 0x02;
    const uint32_t OLD_CONFIG_MAGIC = 0x626C6E6B;

    struct OldWiFiConfigStore {
        uint32_t magic;
        char version[15];
        uint8_t flags;

        char wifiSSID[34];
        char wifiPass[64];

        char cloudToken[34];
        char cloudHost[34];
        uint16_t cloudPort;

        uint32_t staticIP;
        uint32_t staticMask;
        uint32_t staticGW;
        uint32_t staticDNS;
        uint32_t staticDNS2;

        int last_error;

        bool getFlag(uint8_t mask) {
            return (flags & mask) == mask;
        }
    } __attribute__((packed));

    Preferences prefs;
    if (!prefs.begin("blynk", true)) {  // read-only
        return false;
    }
    OldWiFiConfigStore configStore;
    memset(&configStore, 0, sizeof(configStore));
    if (!prefs.getBytes("config", &configStore, sizeof(configStore))) {
        return false;
    }
    // Terminate strings just in case
    configStore.wifiSSID[sizeof(configStore.wifiSSID) - 1] = 0;
    configStore.wifiPass[sizeof(configStore.wifiPass) - 1] = 0;
    configStore.cloudToken[sizeof(configStore.cloudToken) - 1] = 0;
    configStore.cloudHost[sizeof(configStore.cloudHost) - 1] = 0;

    // Validate old config
    if (configStore.magic != OLD_CONFIG_MAGIC) {
        return false;
    }
    if (!configStore.getFlag(OLD_CONFIG_FLAG_VALID)) {
        return false;
    }
    if (strlen(configStore.cloudToken) != 32) {
        return false;
    }
    if (strlen(configStore.cloudHost) == 0) {
        return false;
    }

    String oldUID;
    {
        uint32_t uid[3] = {
            0,
        };
        if (prefs.getBytes("dev_uid", uid, sizeof(uid)) == sizeof(uid)) {
            char str[64];
            const uint8_t* id = (const uint8_t*)uid;
            snprintf(str, sizeof(str), "%02x%02x%02x%02x-%02x%02x%02x%02x-%02x%02x%02x%02x",
                     id[0], id[1], id[2], id[3],
                     id[4], id[5], id[6], id[7],
                     id[8], id[9], id[10], id[11]);
            oldUID = String(str);
        }
    }

    prefs.end();

    /*
     * Migrate old UID
     */
    if (oldUID.length() > 0) {
        LOG_I("Migrating old UID: %s", oldUID.c_str());
        if (prefs.begin("system")) {
            prefs.putString("uid", oldUID);
            prefs.end();
        }
    }

    /*
     * Migrate WiFi and Cloud configuration
     */
#ifdef NetMgr_WiFi
    LOG_I("Migrating old WiFi configuration: SSID=%s", configStore.wifiSSID);
    if (configStore.getFlag(OLD_CONFIG_FLAG_STATIC_IP)) {
        if (!NetMgrWiFi.addNetwork(configStore.wifiSSID, configStore.wifiPass,
                                   configStore.staticIP, configStore.staticGW, configStore.staticMask,
                                   configStore.staticDNS, configStore.staticDNS2)) {
            LOG_E("Failed to migrate WiFi configuration (static IP)");
            return false;
        }
    } else {
        if (!NetMgrWiFi.addNetwork(configStore.wifiSSID, configStore.wifiPass)) {
            LOG_E("Failed to migrate WiFi configuration (DHCP)");
            return false;
        }
    }
#endif

    LOG_I("Migrating old config: Host=%s, Token=%s", configStore.cloudHost, configStore.cloudToken);
    cfg.setBlynkAuth(configStore.cloudToken);
    cfg.setBlynkHost(configStore.cloudHost);
    cfg.commit();
    if (!cfg.isSaved()) {
        LOG_E("Failed to save migrated configuration");
        return false;
    }

    // Remove old config
    if (prefs.begin("blynk")) {
        prefs.remove("config");
        prefs.remove("dev_uid");
        prefs.end();
    }
    LOG_W("Migrating config is complete");
    return true;
}
