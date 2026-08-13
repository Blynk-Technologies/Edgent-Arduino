/*
 * Copyright (c) 2026 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <queue>

#define BLE_IND_TIMEOUT_MS 700

constexpr static char SERVICE_UUID[]            = "95e30001-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_RX[]  = "95e30002-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_TX[]  = "95e30003-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_RAW[] = "95e30004-5737-45a9-a092-a88e2e5dd659";

class BlynkBLE :
    public NimBLEServerCallbacks,
    public NimBLECharacteristicCallbacks
{

public:
    typedef void (*rawDataCb_t)(const uint8_t* data, size_t len);

    rawDataCb_t onRawData = nullptr;

    BlynkBLE()
        : _connected(false)
    {}

    void begin(const char* name) {
        // Create the BLE Device
        NimBLEDevice::init(name);
        _sem_indicate = xSemaphoreCreateCounting(CONFIG_BT_NIMBLE_GATT_MAX_PROCS, CONFIG_BT_NIMBLE_GATT_MAX_PROCS);

#ifdef ESP_PLATFORM
        NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
#else
        NimBLEDevice::setPower(9); /** +9db */
#endif

        // Create the BLE Server
        _server = NimBLEDevice::createServer();
        _server->setCallbacks(this);

        // Create the BLE Service
        if (!(_service = _server->getServiceByUUID(SERVICE_UUID))) {
            _service = _server->createService(SERVICE_UUID);

            // Create a BLE Characteristic
            _tx_char = _service->createCharacteristic(
                                CHARACTERISTIC_UUID_TX,
                                NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::INDICATE);

            _rx_char = _service->createCharacteristic(
                                CHARACTERISTIC_UUID_RX,
                                NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE);

            _raw_char = _service->createCharacteristic(
                                CHARACTERISTIC_UUID_RAW,
                                NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE);

            if (_tx_char) {
                _tx_char->setCallbacks(this);
            }
            if (_rx_char) {
                _rx_char->setCallbacks(this);
            }
            if (_raw_char) {
                _raw_char->setCallbacks(this);
            }
        }

        // Start advertising
        NimBLEAdvertising* adv = _server->getAdvertising();
        adv->enableScanResponse(true);
        adv->addServiceUUID(_service->getUUID());
        adv->setName(name);
        adv->start();
    }

    void end() {
        if(_server) {
            if(auto adv = _server->getAdvertising()) {
                adv->stop();
            }
            auto peers = _server->getPeerDevices();
            for(auto connHandle : peers) {
                _server->disconnect(connHandle);
            }
            _server->setCallbacks(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(300));
        NimBLEDevice::deinit(true); // !!! clearAll

        if(_sem_indicate) {
            vSemaphoreDelete(_sem_indicate);
            _sem_indicate = NULL;
        }
    }

    size_t write(const void* buf, size_t len) {
        if (xSemaphoreTake(_sem_indicate, pdMS_TO_TICKS(BLE_IND_TIMEOUT_MS)) == pdFALSE) {
            LOG_E("Can't lock semaphore in reasonable time");
            return 0;
        }
        _tx_char->indicate((uint8_t*)buf, len);
#if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_DEBUG
        char b[len+1];
        memcpy(b, buf, len);
        b[len] = '\0';
        LOG_D("<< %s", b);
#endif
        return len;
    }

    size_t write(const char* buf) {
        return write(buf, strlen(buf));
    }

    String read() {
      String result;
      {
        char* msg = _rx_queue.front();
        result = msg;
        free(msg);
        _rx_queue.pop();
      }
      return result;
    }

    bool available() {
        return !_rx_queue.empty();
    }

    bool isConnected() {
        return _connected;
    }

    unsigned getMTU() {
        return NimBLEDevice::getMTU();
    }

private:

    void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
        LOG_I("BLE connected");
        _connected = true;
        _server->updateConnParams(connInfo.getConnHandle(), 6, 12, 0, 200);
    }
    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
        LOG_I("BLE disconnected");
        _connected = false;
        _server->getAdvertising()->start();
    }
    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
        LOG_I("MTU updated: %u for connection ID: %u\n", MTU, connInfo.getConnHandle());
    }

    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
      const uint8_t* data = pChar->getValue();
      uint16_t len  = pChar->getLength();

      if (!data || !len) {
        return;
      }

      if (pChar == _rx_char) {
        char* msg = (char*)malloc(len+1);
        memcpy(msg, data, len);
        msg[len] = '\0';   // Null-terminate string
        LOG_D(">> %s", msg);
        {
          _rx_queue.push(msg);
        }
      } else if (pChar == _raw_char) {
        if (onRawData) onRawData(data, len);
      }
    }

    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override {
        if (code == 0 || code == BLE_HS_EDONE) {
        } else {
            LOG_E("Notify/Indicate failed: %d", code);
        }
        xSemaphoreGive(_sem_indicate);
    }

private:
    bool                    _connected;
    std::queue<char*>       _rx_queue;
    NimBLEServer            *_server;
    NimBLEService           *_service;
    NimBLECharacteristic    *_tx_char;
    NimBLECharacteristic    *_rx_char;
    NimBLECharacteristic    *_raw_char;
    SemaphoreHandle_t       _sem_indicate = NULL;
};

