# Blynk.Edgent for Arduino IDE

**Blynk.Edgent** is a library for tinkerers that makes it easy to connect their devices to the Blynk IoT platform
and take advantage of its advanced features without extensive coding.

![image](https://github.com/blynkkk/blynkkk.github.io/raw/master/images/GithubBanner.jpg?raw=1)

## Features

- **Blynk.Inject**: connect your devices easily using [**Blynk IoT App**][blynk-apps] (iOS and Android)
  - `BLE`-assisted device provisioning for the best end-user experience
  - `WiFiAP`-based provisioning for devices without BLE support
- **Network Manager**: Advanced network connection management and troubleshooting
  - `WiFi`: Maintains connection to the most reliable WiFi network (up to 16 configured networks)
  <!--
  - `Ethernet`: Supports `Static IP` or `DHCP` network configuration
  - `Cellular`: Provides connectivity through `2G GSM`, `EDGE`, `3G`, `4G LTE`, `Cat M1`, or `5G` networks using `PPP`
  -->
- Secure **Blynk.Cloud** connection that provides simple API for:
  - Data transfer with `Virtual Pins`, reporting `Events`, and accessing `Metadata`
- **Blynk.Air** - automatic, managed Over The Air firmware updates using Web Dashboard
  - Direct firmware upgrade using iOS/Android App before device activation

Supported devices:
```
✅ ESP32, ESP32-S2, ESP32-S3, ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6
✅ ESP8266
✅ Seeed Wio Terminal
```

## Getting Started

- Sign up/Log in to your [Blynk Account](https://blynk.cloud)
- Install **Blynk IoT App** for [iOS][blynk-ios] or [Android][blynk-android]
- Create a new Product Template. This will provide you with `Template ID` and `Template Name`.

## 1. Prepare your sketch

In Arduino IDE menu: `File` -> `Examples` -> `BlynkEdgent` -> `BlynkEdgent`.

Follow instructions in the example sketch.

Verify and Upload!


## 2. Connect your device to Blynk.Cloud

1. Open **Blynk IoT App** on your smartphone
2. Click **Add device** -> **Find devices nearby**
3. Select your device and follow the wizard instructions

The device should appear `online` once the above steps complete successfully.

## 3. Update your device using OTA

1. Save your sketch.
2. In the Arduino IDE, select `Sketch` -> `Export Compiled Binary`.
3. A `build` subfolder should be created in your sketch folder. Find the firmware file named `<YourSketchName>.ino.bin` (for example, `BlynkEdgent.ino.bin`).
4. In the Blynk Web Console, open your device, click `...` under the device name, and select `Update Firmware`.
5. Under `Apply update if the device has...`, select `Another build date`.  
   Note: For this example, we use this option to force the update. In normal use, using firmware version criteria (by setting `BLYNK_FIRMWARE_VERSION`) is recommended.
6. Upload the firmware file. The firmware should be detected automatically, and its information should be displayed.
7. Click `Start Shipping`.


# Further reading

- [Blynk.Edgent](https://docs.blynk.io/en/blynk.edgent/overview)
- [Blynk IoT Apps][blynk-apps]
- [Deploying Products With Dynamic AuthTokens](https://docs.blynk.io/en/commercial-use/deploying-products-with-dynamic-authtokens)
- [Blynk Network Co-Processor (NCP)](https://docs.blynk.io/en/blynk.ncp/overview)


[blynk-apps]: https://docs.blynk.io/en/downloads/blynk-apps-for-ios-and-android
[blynk-android]: https://play.google.com/store/apps/details?id=cloud.blynk
[blynk-ios]: https://apps.apple.com/us/app/blynk-iot/id1559317868
[blynk_sales]: https://blynk.io/en/contact-us-business
