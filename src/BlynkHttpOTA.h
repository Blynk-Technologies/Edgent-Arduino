/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ArduinoHttpClient.h>
#include <updater/BlynkUpdater.h>

static
bool parseURL(String url, String& protocol, String& host, int& port, String& path)
{
  int index = url.indexOf(':');
  if (index < 0) {
    return false;
  }

  protocol = url.substring(0, index);
  url.remove(0, (index + 3)); // remove protocol part

  index = url.indexOf('/');
  String server = url.substring(0, index);
  url.remove(0, index);       // remove server part

  index = server.indexOf(':');
  if (index >= 0) {
    host = server.substring(0, index);          // hostname
    port = server.substring(index + 1).toInt(); // port
  } else {
    host = server;
    if (protocol == "http") {
      port = 80;
    } else if (protocol == "https") {
      port = 443;
    }
  }

  if (url.length()) {
    path = url;
  } else {
    path = "/";
  }
  return true;
}

static
bool downloadUpgradeHTTP(HttpClient& http)
{
  // Send custom headers if needed
  //http.sendHeader("X-CUSTOM-HEADER", "custom_value");
  //http.endRequest();

  const int statusCode = http.responseStatusCode();
  if (statusCode < 200 || statusCode > 299) {
    LOG_E("HTTP status code: %d", statusCode);
    return false;
  }

  String expectedSHA256;

  while (http.headerAvailable()) {
    String hdr = http.readHeaderName();
    String val = http.readHeaderValue();
    hdr.toLowerCase();
    if (hdr == "x-sha256") {
      expectedSHA256 = val;
    }
  }

  const int contentLength = http.contentLength();
  if (contentLength < 1024) {
    LOG_E("Content-Length not defined or too small");
    return false;
  }

  if (!expectedSHA256.length()) {
    LOG_E("X-SHA256 not defined");
    return false;
  }

  if (!ArduinoUpdate.begin(contentLength)) {
    LOG_E("Not enough space to store the update");
    return false;
  }

  if (!ArduinoUpdate.setSHA256(expectedSHA256.c_str())) {
    LOG_E("Invalid X-SHA256 value: %s", expectedSHA256.c_str());
    ArduinoUpdate.abort();
    return false;
  }

  int written = 0;
  int prevProgress = 0;
  uint8_t buff[256];
  uint32_t tLastRead = millis();

  while ((written < contentLength) &&
         ((millis() - tLastRead) < 30000L))
  {
    int len = http.readBytes(buff, sizeof(buff));
    if (len <= 0) {
      delay(10);
      continue;
    }

    if (!ArduinoUpdate.write(buff, len)) {
      ArduinoUpdate.abort();
      return false;
    }

    tLastRead = millis();
    written += len;

    const int progress = (written*100)/contentLength;
    if (progress - prevProgress >= 1 || progress == 100) {
#ifdef BLYNK_PRINT
      BLYNK_PRINT.print("\r Flashing... ");
      BLYNK_PRINT.print(progress);
      BLYNK_PRINT.print("%");
#endif
      prevProgress = progress;
    }
  }
#ifdef BLYNK_PRINT
  BLYNK_PRINT.println();
#endif

  if (written != contentLength) {
    LOG_E("Interrupted at %d / %d bytes", written, contentLength);
    ArduinoUpdate.abort();
    return false;
  }

  if (ArduinoUpdate.end()) {
    LOG_I("=== Update successfully completed. Rebooting.");
    ArduinoUpdate.apply();
    return true; // should not happen (device reboots)
  } else {
    LOG_E("Firmware validation failed: %s", ArduinoUpdate.errorString().c_str());
    ArduinoUpdate.abort();
    return false;
  }
}

static
bool downloadUpgrade(const String& url)
{
  String protocol, host, path;
  int port = 0;

  LOG_I("OTA: %s", url.c_str());

  if (!parseURL(url, protocol, host, port, path)) {
    LOG_E("Cannot parse URL");
    return false;
  }

  // Disconnect, not to interfere with OTA process
  Blynk.disconnect();

  LOG_I("Connecting to %s:%d", host.c_str(), port);

  Client* client = getConnectedClient(protocol, host, port);
  if (!client) {
    LOG_E("Cannot connect");
    return false;
  }

  HttpClient http(*client, host, port);
  http.connectionKeepAlive();  // Use the established connection
  http.get(path);

  return downloadUpgradeHTTP(http);
}

bool uploadCoreDump(const String& server, const String& token, size_t dumpSize) {
#if defined(ESP32)
  WiFiClientSecure net;
  net.setCACert(ROOT_CA_CERT);

  HttpClient http(net, server.c_str(), 443);
  http.setHttpResponseTimeout(30 * 1000);

  const char* boundary = "blynk-esp32-coredump";

  String head;
  head.reserve(180);
  head += "--";
  head += boundary;
  head += "\r\n";
  head += "Content-Disposition: form-data; name=\"upfile\"; filename=\"coredump.bin\"\r\n";
  head += "Content-Type: application/octet-stream\r\n";
  head += "\r\n";

  String tail;
  tail += "\r\n--";
  tail += boundary;
  tail += "--\r\n";

  const size_t contentLength = head.length() + dumpSize + tail.length();

  http.beginRequest();
  http.post(String("/external/api/upload?type=DUMP&token=") + token);
  http.sendHeader("Host", server);
  http.sendHeader("User-Agent", "ESP32-Arduino");
  http.sendHeader("Connection", "close");

  String contentType = "multipart/form-data; boundary=";
  contentType += boundary;
  http.sendHeader("Content-Type", contentType);
  http.sendHeader("Content-Length", contentLength);

  http.beginBody();

  http.print(head);

  systemCoreDumpWrite(http);

  http.print(tail);

  http.endRequest();

  systemCoreDumpClear();

  const int status = http.responseStatusCode();
  http.stop();

  return status >= 200 && status < 300;
#else
  (void)server;
  (void)token;
  (void)dumpSize;
  return false;
#endif
}
