/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <queue>

constexpr static char SERVICE_UUID[] = "95e30001-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_RX[] = "95e30002-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_TX[] = "95e30003-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_RAW[] = "95e30004-5737-45a9-a092-a88e2e5dd659";

class BlynkBLE : public BLEServerCallbacks,
                 public BLECharacteristicCallbacks {

public:
    typedef void (*rawDataCb_t)(const uint8_t* data, size_t len);

    rawDataCb_t onRawData = nullptr;

    BlynkBLE()
        : _connected(false) {}

    void begin(const char* name) {
        // Create the BLE Device
        BLEDevice::init(name);

        if (_server) return;

        // Create the BLE Server
        _server = BLEDevice::createServer();
        _server->setCallbacks(this);

        // Create the BLE Service
        _service = _server->createService(SERVICE_UUID);

        // Create a BLE Characteristic
        _tx_char = _service->createCharacteristic(
            CHARACTERISTIC_UUID_TX,
            BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);

        _tx_char->addDescriptor(new BLE2902());

        _rx_char = _service->createCharacteristic(
            CHARACTERISTIC_UUID_RX,
            BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_WRITE);

        _raw_char = _service->createCharacteristic(
            CHARACTERISTIC_UUID_RAW,
            BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_WRITE);

        _rx_char->setCallbacks(this);
        _raw_char->setCallbacks(this);

        // Start the service
        _service->start();

        // Start advertising
        _server->getAdvertising()->addServiceUUID(_service->getUUID());
        _server->getAdvertising()->setScanResponse(true);
        _server->getAdvertising()->start();
    }

    void end() {
        _server->getAdvertising()->stop();
    }

    size_t write(const void* buf, size_t len) {
        _tx_char->setValue((uint8_t*)buf, len);
        _tx_char->indicate();

        //Serial.write("<< ");
        //Serial.write((uint8_t*)buf, len);
        //Serial.println();

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
        return BLEDevice::getMTU(); // TODO: always returns 23?
    }

private:
    void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override {
        //Serial.println("BLE connected");
        _connected = true;
        server->updateConnParams(param->connect.remote_bda, 6, 12, 0, 200);
    }
    void onDisconnect(BLEServer* server) override {
        //Serial.println("BLE disconnected");
        _server->getAdvertising()->start();
        _connected = false;
    }

    void onWrite(BLECharacteristic* pChar) override {
        const uint8_t* data = (const uint8_t*)pChar->getValue().data();
        uint16_t len = pChar->getLength();

        if (!data || !len) {
            return;
        }

        if (pChar == _rx_char) {
        //Serial.write(">> ");
        //Serial.write(data, len);
        //Serial.println();

            char* msg = (char*)malloc(len + 1);
            memcpy(msg, data, len);
            msg[len] = 0;   // Null-terminate string
            {
                _rx_queue.push(msg);
            }
        } else if (pChar == _raw_char) {
            if (onRawData) onRawData(data, len);
        }
    }

private:
    bool _connected;
    std::queue<char*> _rx_queue;
    BLEServer* _server;
    BLEService* _service;
    BLECharacteristic* _tx_char;
    BLECharacteristic* _rx_char;
    BLECharacteristic* _raw_char;
};
