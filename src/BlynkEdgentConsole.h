/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#if defined(ESP32)
extern "C" {
  #include "esp_partition.h"
  #include "esp_ota_ops.h"
}
#endif

#ifdef CONFIG_COMMAND_I2CDETECT
  #include <Wire.h>
#endif

void Edgent::initConsole(Stream& stream) {
    _console.begin(stream);
    _console.print("\n>");
}

void Edgent::initConsoleCommands() {
    _console.addCommand("reboot", [this]() {
        if (_onUserInitiatedReboot) {
            _onUserInitiatedReboot();
        }
        _console.print("Rebooting...\n");
        systemReboot();
    });

    _console.addCommand("devinfo", [this]() {
        _console.printf(
            R"json({"name":"%s","board":"%s","tmpl_id":"%s","fw_type":"%s","fw_ver":"%s"})json"
            "\n",
            systemGetFullName().c_str(),
            BLYNK_TEMPLATE_NAME,
            BLYNK_TEMPLATE_ID,
            BLYNK_FIRMWARE_TYPE,
            BLYNK_FIRMWARE_VERSION);
    });

    _console.addCommand("connect", [this](const BlynkParam& param) {
        if (!param[0].isValid()) {
            _console.print("Usage: connect <auth> [host]\n");
            return;
        }
        String auth = param[0].asStr();
        if (auth.length() != 32) {
            _console.print("Error: invalid token size\n");
            return;
        }

        _store.loadDefault();
        _store.setBlynkAuth(auth);

        if (param[1].isValid()) {
            String host = param[1].asStr();
            _store.setBlynkHost(host);
        }

        _console.print("Trying to connect...\n");
        startInitialConnection();
    });

    _console.addCommand("config", [this](const BlynkParam& param) {
        const String cmd = param[0].asStr();
        if (!param[0].isValid() || cmd == "start") {
            startConfig();
        } else if (cmd == "stop") {
            stopConfig();
        } else if (cmd == "erase") {
            NetMgr.clearAllNetworks();
            setState(MODE_RESET_CONFIG);
        } else {
            _console.getStream().println(F("Available commands: start, stop, erase"));
        }
    });

    _console.addCommand("firmware", [this](const BlynkParam& param) {
        const String cmd = param[0].asStr();
        if (!param[0].isValid() || cmd == "info") {
            _console.printf(" Version:   %s (build %s)\n", BLYNK_FIRMWARE_VERSION, __DATE__ " " __TIME__);
            _console.printf(" Type:      %s\n", BLYNK_FIRMWARE_TYPE);
            _console.printf(" Platform:  %s\n", BLYNK_INFO_DEVICE);
#if defined(ESP32)
            _console.printf(" SDK:       %s\n", ESP.getSdkVersion());

            if (const esp_partition_t* running = esp_ota_get_running_partition()) {
                unsigned sketchSize = ESP.getSketchSize();
                _console.printf(" Partition: %s (%dK)\n", running->label, running->size / 1024);
                _console.printf(" App size:  %dK (%d%%)\n", sketchSize / 1024, (sketchSize * 100) / (running->size));
                _console.printf(" App MD5:   %s\n", ESP.getSketchMD5().c_str());
            }
#elif defined(ESP8266)
      unsigned sketchSize = ESP.getSketchSize();
      unsigned partSize = sketchSize + ESP.getFreeSketchSpace();

      _console.printf(" SDK:       %s\n", ESP.getSdkVersion());
      _console.printf(" ESP Core:  %s\n", ESP.getCoreVersion().c_str());

      _console.printf(" App size:  %dK (%d%%)\n", sketchSize/1024, (sketchSize*100)/partSize);
      _console.printf(" App MD5:   %s\n", ESP.getSketchMD5().c_str());
#elif defined(ARDUINO_ARCH_RP2040)
      _console.printf(" SDK:       %s\n", PICO_SDK_VERSION_STRING);
#elif defined(PARTICLE)
      _console.printf(" Device OS: %s\n", System.version().c_str());
#endif
        } else if (cmd == "rollback") {
            if (ArduinoUpdate.canRollback()) {
                _console.print("Rolling back...\n");
                ArduinoUpdate.rollback();
            } else {
                _console.print("Rollback not available\n");
            }
        } else {
            _console.getStream().println(F("Available commands: info, rollback"));
        }
    });

#if defined(CONFIG_COMMAND_SYS)
    _console.addCommand("sys", [this](const BlynkParam& param) {
        const String tool = param[0].asStr();
        if (tool == "info") {
            _console.printf(" Uptime:          %s\n", timeSpanToStr(systemUptime() / 1000).c_str());
            _console.printf(" Reset reason:    %s\n", systemGetResetReason().c_str());
            _console.printf(" Reboots total:   %lu\n", systemStats.resetCount.total);
            _console.printf("      graceful:   %lu\n", systemStats.resetCount.graceful);
            _console.printf(" Network drops:   %d\n", systemStats.network_drops);
            _console.printf(" Cloud drops:     %d\n", systemStats.cloud_drops);
            _console.printf(" Online total:    %s\n", timeSpanToStr(systemStats.total_online_time).c_str());
            _console.printf("          max:    %s\n", timeSpanToStr(systemStats.max_online_time).c_str());
            _console.printf(" Offline total:   %s\n", timeSpanToStr(systemStats.total_offline_time).c_str());
            _console.printf("           max:   %s\n", timeSpanToStr(systemStats.max_offline_time).c_str());
  #if defined(ESP32)
            _console.printf(" Chip:            %s rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
            _console.printf(" Flash:           %dK, %luM, %s\n", ESP.getFlashChipSize() / 1024,
                            ESP.getFlashChipSpeed() / 1000000,
                            systemGetFlashMode().c_str());
            _console.printf(" Stack unused:    %d\n", uxTaskGetStackHighWaterMark(NULL));
            _console.printf(" Heap free:       %d / %d\n", ESP.getFreeHeap(), ESP.getHeapSize());
            _console.printf("      max alloc:  %d\n", ESP.getMaxAllocHeap());
            _console.printf("      min free:   %d\n", ESP.getMinFreeHeap());
            if (ESP.getPsramSize()) {
                _console.printf(" PSRAM free:      %d / %d\n", ESP.getFreePsram(), ESP.getPsramSize());
                _console.printf("      max alloc:  %d\n", ESP.getMaxAllocPsram());
                _console.printf("      min free:   %d\n", ESP.getMinFreePsram());
            }
  #elif defined(ESP8266)
      uint32_t heap_free, heap_max;
      uint8_t heap_frag;
      ESP.getHeapStats(&heap_free, &heap_max, &heap_frag);

      _console.printf(" Flash:           %dK, %luM, %s\n", ESP.getFlashChipSize() / 1024,
                                                          ESP.getFlashChipSpeed() / 1000000,
                                                          systemGetFlashMode().c_str());
      _console.printf(" Stack unused:    %d\n",        ESP.getFreeContStack());
      _console.printf(" Heap free:       %d\n",        heap_free);
      _console.printf("      fragment:   %d\n",        heap_frag);
      _console.printf("      max alloc:  %d\n",        heap_max);
  #elif defined(ARDUINO_AMEBA)
      _console.printf(" Heap free:       %d\n",        os_get_free_heap_size_arduino());
  #endif

  #if defined(BLYNK_USE_LITTLEFS)
    #if defined(ESP32)
            uint32_t fs_total = LittleFS.totalBytes();
            _console.printf(" FS free:         %d / %d\n", (fs_total - LittleFS.usedBytes()), fs_total);
    #elif defined(ESP8266)
      FSInfo fs_info;
      LittleFS.info(fs_info);
      _console.printf(" FS free:         %d / %d\n",   (fs_info.totalBytes-fs_info.usedBytes), fs_info.totalBytes);
    #endif
  #endif

  #if defined(ESP32)
        } else if (tool == "coredump") {
            const String cmd = param[1].asStr();
            if (!param[1].isValid() || cmd == "info") {
                if (size_t coredump_size = systemCoreDumpSize()) {
                    _console.printf("Core dump available (%d bytes)\n", coredump_size);
                } else {
                    _console.printf("No core dump available\n");
                }
            } else if (cmd == "clear") {
                systemCoreDumpClear();
            } else if (cmd == "test") {
                const String cmd2 = param[2].asStr();
                if (cmd2 == "assert") {
                    assert(false);
                } else if (cmd2 == "crash") {
                    *((volatile uint32_t*)0) = 0;  // trigger crash
                }
            }
        } else if (tool == "partitions") {
            esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
            if (it) {
                do {
                    const esp_partition_t* p = esp_partition_get(it);
                    _console.printf("|  %02x  | %02x  | 0x%06X | 0x%06X | %-16s |\n",
                                    p->type, p->subtype, p->address, p->size, p->label);
                } while ((it = esp_partition_next(it)));
            }
  #endif
  #if defined(ESP32) || defined(ESP8266)
        } else if (tool == "powersave") {
            const String cmd = param[1].asStr();
            if (!param[1].isValid()) {
                _console.printf("WiFi powersave: %s\n", WiFi.getSleep() ? "on" : "off");
            } else if (cmd == "on") {
                WiFi.setSleep(true);
            } else if (cmd == "off") {
                WiFi.setSleep(false);
            }
  #endif
  #if defined(ESP32)
        } else if (tool == "cpufreq") {
            const String cmd = param[1].asStr();
            if (!param[1].isValid()) {
                _console.printf("CPU freq: %lu MHz\n", getCpuFrequencyMhz());
            } else {
                const unsigned freq = param[1].asInt();
                if (freq == 10 || freq == 20 || freq == 40 ||
                    freq == 80 || freq == 160 || freq == 240) {
                    setCpuFrequencyMhz(freq);
                }
            }
  #endif
        } else if (tool == "drop_stats") {
            systemStats.clear();
        } else {
            _console.getStream().println(F("Available commands: info, coredump [info|clear], partitions, powersave [on|off], nodelay [on|off], cpufreq [N MHz], drop_stats"));
        }
    });
#endif // CONFIG_COMMAND_SYS

#if defined(CONFIG_COMMAND_NETMGR) && defined(NetMgr_WiFi)
    _console.addCommand("wifi", [this](const BlynkParam& param) {
        const String cmd = param[0].asStr();
        if (!param[0].isValid() || cmd == "info") {
            _console.printf("mac:%s ip:%s status:%s\n",
                            NetMgrWiFi.getMacAddress().c_str(),
                            NetMgrWiFi.getLocalIP().c_str(),
                            NetMgrWiFi.getStatus().c_str());
            if (NetMgrWiFi.getStatus() == "ready") {
                _console.printf("ssid:%s bssid:%s rssi:%ddBm\n",
                                NetMgrWiFi.getNetworkSSID().c_str(),
                                NetMgrWiFi.getNetworkBSSID().c_str(),
                                NetMgrWiFi.getRSSI());
            }
            if (NetMgrWiFi.getErrorStr()) {
                _console.printf("error: %s\n", NetMgrWiFi.getErrorStr());
            }
        } else if (cmd == "scan") {
            int found = NetMgrWiFi.scanNetworks();
            if (found <= 0) {
                _console.printf("No networks\n");
            }
            for (int i = 0; i < found; i++) {
                String ssid, sec, bssid;
                int chan = -1, rssi = 0;
                if (NetMgrWiFi.scanGetResult(i, ssid, sec, rssi, bssid, chan)) {
                    bool current = (bssid == NetMgrWiFi.getNetworkBSSID());
                    _console.printf(
                        "%s %-20s [%s] %s ch:%d rssi:%d\n",
                        (current ? "*" : " "),
                        ssid.c_str(), bssid.c_str(), sec.c_str(),
                        chan, rssi);
                }
            }
            NetMgrWiFi.scanDelete();
        } else if (cmd == "add") {
            if (param[2].isValid()) {
                NetMgrWiFi.addNetwork(param[1].asStr(), param[2].asStr());
            } else {
                NetMgrWiFi.addNetwork(param[1].asStr());
            }
        } else if (cmd == "clear") {
            NetMgrWiFi.clearNetworks();
        } else if (cmd == "on") {
            NetMgrWiFi.on();
        } else if (cmd == "off") {
            NetMgrWiFi.off();
        } else {
            _console.getStream().println(F("Available commands: info, scan, add ssid [pass], clear, on, off"));
        }
    });
#endif /* NetMgr_WiFi */

#if defined(CONFIG_COMMAND_NETMGR) && defined(NetMgr_Ethernet)
    _console.addCommand("eth", [this](const BlynkParam& param) {
        const String cmd = param[0].asStr();
        if (!param[0].isValid() || cmd == "info") {
            if (!NetMgrEthernet.isHardwareAvailable()) {
                _console.printf("Hardware N/A\n");
                return;
            }
            _console.printf("mac:%s ip:%s status:%s\n",
                            NetMgrEthernet.getMacAddress().c_str(),
                            NetMgrEthernet.getLocalIP().c_str(),
                            NetMgrEthernet.getStatus().c_str());
            if (NetMgrEthernet.getErrorStr()) {
                _console.printf("error: %s\n", NetMgrEthernet.getErrorStr());
            }
        } else if (cmd == "on") {
            NetMgrEthernet.on();
        } else if (cmd == "off") {
            NetMgrEthernet.off();
        } else {
            _console.getStream().println(F("Available commands: info, on, off"));
        }
    });
#endif /* NetMgr_Ethernet */

#if defined(CONFIG_COMMAND_NETMGR) && defined(NetMgr_Cellular)
    _console.addCommand("cell", [this](const BlynkParam& param) {
        const String cmd = param[0].asStr();
        if (!param[0].isValid() || cmd == "info") {
            if (!NetMgrCellular.isHardwareAvailable()) {
                _console.printf("Hardware N/A\n");
                return;
            }
            _console.printf("imei:%s ip:%s status:%s\n",
                            NetMgrCellular.getIMEI().c_str(),
                            NetMgrCellular.getLocalIP().c_str(),
                            NetMgrCellular.getStatus().c_str());
            if (NetMgrCellular.getStatus() == "ready") {
                _console.printf("operator:%s signal:%d%%\n",
                                NetMgrCellular.getOperator().c_str(),
                                NetMgrCellular.getSignalStrength());
            }
            if (NetMgrCellular.getErrorStr()) {
                _console.printf("error: %s\n", NetMgrCellular.getErrorStr());
            }
        } else if (cmd == "on") {
            NetMgrCellular.on();
        } else if (cmd == "off") {
            NetMgrCellular.off();
        } else if (cmd == "modem") {
            const String cmd2 = param[1].asStr();
            if (!param[1].isValid() || cmd2 == "info") {
                String name = NetMgrCellular.getModemName();
                String info = NetMgrCellular.getModemInfo();
                String imei = NetMgrCellular.getIMEI();
                String imsi = NetMgrCellular.getIMSI();
                String iccid = NetMgrCellular.getICCID();

                _console.printf("Modem: %s\n", name.c_str());
                _console.printf("       %s\n", info.c_str());
                _console.printf("IMEI:  %s\n", imei.c_str());
                _console.printf("IMSI:  %s\n", imsi.c_str());
                _console.printf("ICCID: %s\n", iccid.c_str());
            }
        } else {
            _console.getStream().println(F("Available commands: info, on, off, modem"));
        }
    });
#endif /* NetMgr_Cellular */

#if defined(CONFIG_COMMAND_PREFS)
    _console.addCommand("prefs", [this](int argc, const char** argv) {
        if (argc < 1) {
      // do nothing
        } else if (0 == strcmp(argv[0], "set") && argc == 4) {
            Preferences prefs;
            prefs.begin(argv[1]);
            if (prefs.putString(argv[2], argv[3])) {
                _console.print(R"json({"status":"ok"})json"
                               "\n");
            } else {
                _console.print(R"json({"status":"error"})json"
                               "\n");
            }
            return;
        } else if (0 == strcmp(argv[0], "get") && argc == 3) {
            if (String(argv[1]) == "blynk" && String(argv[2]) == "auth") {
                _console.print(R"json({"status":"error","msg":"not allowed"})json"
                               "\n");
                return;
            }
            Preferences prefs;
            prefs.begin(argv[1], true); // readonly
            if (prefs.isKey(argv[2])) {
                String val = prefs.getString(argv[2], "");
                _console.printf(R"json({"value":"%s"})json"
                                "\n",
                                val.c_str());
            } else {
                _console.print(R"json({"status":"error","msg":"not found"})json"
                               "\n");
            }
            return;
        } else if (0 == strcmp(argv[0], "erase") && argc >= 2) {
            Preferences prefs;
            prefs.begin(argv[1]);
            bool status = ((argc == 2) ? prefs.clear() : prefs.remove(argv[2]));
            if (status) {
                _console.print(R"json({"status":"ok"})json"
                               "\n");
            } else {
                _console.print(R"json({"status":"error"})json"
                               "\n");
            }
            return;
        }

        _console.print(R"json({"status":"error","msg":"invalid args"})json"
                       "\n");
    });
#endif /* CONFIG_COMMAND_PREFS */

#if defined(CONFIG_COMMAND_I2CDETECT)
    _console.addCommand("i2cdetect", [this](const BlynkParam& param) {
        Stream& out = _console.getStream();
        uint8_t first = 0x03, last = 0x77;

    // table header
        out.print("  ");
        for (uint8_t i = 0; i < 16; i++) {
            out.printf(" x%X", i);
        }

    // table body
        for (uint8_t address = 0x00; address <= 0x77; address++) {
            if (address % 16 == 0) {
                out.printf("\n%Xx", address >> 4);
            }
            if (address >= first && address <= last) {
                Wire.beginTransmission(address);
                uint8_t error = Wire.endTransmission();
                if (error == 0) {
          // device found
                    out.printf(" %02x", address);
                } else if (error == 4) {
          // other error
                    out.print(" XX");
                } else {
          // error = 2: received NACK on transmit of address
          // error = 3: received NACK on transmit of data
                    out.print(" --");
                }
            } else {
        // address not scanned
                out.print("   ");
            }
        }
        out.println("\ndone.");
    });
#endif /* CONFIG_COMMAND_I2CDETECT */

#if defined(CONFIG_COMMAND_FILESYS) && defined(BLYNK_FS)
    _console.addCommand("ls", [this](int argc, const char** argv) {
        const char* path = (argc < 1) ? "/" : argv[0];
        File rootDir = BLYNK_FS.open(path);
        while (File f = rootDir.openNextFile()) {
  #if defined(BLYNK_USE_SPIFFS) && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0))
            String fn = f.name();
  #else
      String fn = f.path();
  #endif

            MD5Builder md5;
            md5.begin();
            md5.addStream(f, f.size());
            md5.calculate();
            String md5str = md5.toString();

            _console.printf("%8d %-24s %s\n",
                            f.size(), fn.c_str(),
                            md5str.substring(0, 8).c_str());
        }
    });

    _console.addCommand("rm", [this](int argc, const char** argv) {
        if (argc < 1) return;

        for (int i = 0; i < argc; i++) {
            const char* fn = argv[i];
            if (BLYNK_FS.remove(fn)) {
                _console.printf("Removed %s\n", fn);
            } else {
                _console.printf("Removing %s failed\n", fn);
            }
        }
    });

    _console.addCommand("mv", [this](int argc, const char** argv) {
        if (argc != 2) return;

        if (!BLYNK_FS.rename(argv[0], argv[1])) {
            _console.print("Rename failed\n");
        }
    });

    _console.addCommand("cat", [this](int argc, const char** argv) {
        if (argc != 1) return;

        if (!BLYNK_FS.exists(argv[0])) {
            _console.print("File not found\n");
            return;
        }

        if (File f = BLYNK_FS.open(argv[0], FILE_READ)) {
            while (f.available()) {
                _console.print((char)f.read());
            }
            _console.print("\n");
        } else {
            _console.print("Cannot open file\n");
        }
    });

    _console.addCommand("echo", [this](int argc, const char** argv) {
        if (argc != 2) return;

        if (File f = BLYNK_FS.open(argv[1], FILE_WRITE)) {
            if (!f.print(argv[0])) {
                _console.print("Cannot write file\n");
            }
        } else {
            _console.print("Cannot open file\n");
        }
    });
#endif /* CONFIG_COMMAND_FILESYS */
}
