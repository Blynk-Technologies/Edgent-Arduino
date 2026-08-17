#include <Particle.h>
#include <queue>

#if !defined(PARTICLE)
  #error "ConfigSparkBLE.h should be used on Particle platform"
#endif

#if !Wiring_BLE
  #error "Blynk.Inject works using BLE, the target platform lacks BLE support"
#endif

constexpr static char SERVICE_UUID[] = "95e30001-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_RX[] = "95e30002-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_TX[] = "95e30003-5737-45a9-a092-a88e2e5dd659";

class BlynkBLE {

public:
    BlynkBLE() {}

    void begin(const char* name) {
        BLE.on();

        if (!_queue_mutex) {
            _queue_mutex = new Mutex();

            _tx_char = new BleCharacteristic(nullptr,
                                             BleCharacteristicProperty::NOTIFY,
                                             CHARACTERISTIC_UUID_TX, SERVICE_UUID);
            _rx_char = new BleCharacteristic(nullptr,
                                             BleCharacteristicProperty::WRITE_WO_RSP | BleCharacteristicProperty::WRITE,
                                             CHARACTERISTIC_UUID_RX, SERVICE_UUID, ble_data_callback, this);

            BLE.addCharacteristic(*_tx_char);
            BLE.addCharacteristic(*_rx_char);
        }

        BleAdvertisingData advData, scanRspData;
        advData.appendServiceUUID(SERVICE_UUID);

        // Construct Scan Responce Data manually to bypass Particle limitation
        //unsigned name_len = strlen(name);
        //uint8_t buffer[32];
        //buffer[0] = name_len+1;
        //buffer[1] = BLE_SIG_AD_TYPE_COMPLETE_LOCAL_NAME;
        //memcpy(&buffer[2], name, name_len);
        //scanRspData.set(buffer, name_len+2);

        // A bit cleaner way:
        scanRspData.clear();
        scanRspData.append(BleAdvertisingDataType::COMPLETE_LOCAL_NAME,
                           (const uint8_t*)name, strlen(name));

        // TODO: set device name in GENERIC_ACCESS_SVC->DEVICE_NAME_CHAR
        // Particle seemingly lacks API to do that

        BLE.advertise(&advData, &scanRspData);
    }

    void end() {
        BLE.off();
    }

    size_t write(const void* buf, size_t len) {
        _tx_char->setValue((uint8_t*)buf, len);

        LOG_D("<< %s", buf);
        return len;
    }

    size_t write(const char* buf) {
        unsigned len = strlen(buf);
        return write(buf, len);
    }

    String read() {
        String result;
        WITH_LOCK(*_queue_mutex) {
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
        return BLE.connected();
    }

private:
    static void ble_data_callback(const uint8_t* data, size_t len,
                                  const BlePeerDevice& peer, void* self) {
        ((BlynkBLE*)self)->onWrite(data, len);
    }

    void onWrite(const uint8_t* data, size_t len) {
        if (data && len > 0) {
            char* msg = (char*)malloc(len + 1);
            memcpy(msg, data, len);
            msg[len] = 0;   // Null-terminate string
            LOG_D(">> %s", msg);
            WITH_LOCK(*_queue_mutex) {
                _rx_queue.push(msg);
            }
        }
    }

private:
    std::queue<char*> _rx_queue;
    Mutex* _queue_mutex = nullptr;
    BleCharacteristic* _rx_char = nullptr;
    BleCharacteristic* _tx_char = nullptr;
};
