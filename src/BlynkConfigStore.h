/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Preferences.h>

#define BLYNK_PREFS_NAMESPACE "blynk"

struct ConfigStore {

  bool begin() {
    return loadPrefs();
  }

  /*
   * Getters
   */

  int           getConfigSkipped() const {  return _cfgskip; }
  const String& getFirmwareVer() const  {  return _fwver;   }
  const String& getBlynkAuth() const    {  return _auth;    }
  const String& getBlynkHost() const    {  return _host;    }

  bool isConfigured() const {
    return (_auth.length() == 32) && isSaved();
  }

  bool isSaved() const {
    return _saved;
  }

  /*
   * Setters
   */

  void storeConfigSkipped() {
    _cfgskip++;
    Preferences prefs;
    if (prefs.begin(BLYNK_PREFS_NAMESPACE)) {
      prefs.putString("cfgskip", String(_cfgskip));
    }
  }

  void storeFirmwareVer(const String& ver) {
    _fwver = ver;
    Preferences prefs;
    if (prefs.begin(BLYNK_PREFS_NAMESPACE)) {
      prefs.putString("fwver", _fwver);
    }
  }

  void setBlynkAuth(const String& auth) {
    _auth = auth;
    _saved = false;
  }

  void setBlynkHost(const String& host) {
    _host = host;
    _saved = false;
  }

  void loadDefault() {
    _saved = false;
    _cfgskip = 0;
    _fwver = BLYNK_FIRMWARE_VERSION;
    _auth = "invalid token";
    _host = BLYNK_DEFAULT_SERVER;
  }

  void commit() {
    Preferences prefs;
    if (prefs.begin(BLYNK_PREFS_NAMESPACE)) {
      if (!prefs.putString("auth",  _auth)) {
        return;
      }
      if (!prefs.putString("host",  _host)) {
        return;
      }
      _saved = true;
    } else {
      LOG_E("Config write failed");
    }
  }

  void erase() {
    Preferences prefs;
    if (prefs.begin(BLYNK_PREFS_NAMESPACE)) {
      if (!prefs.clear()) {
        prefs.remove("cfgskip");
        prefs.remove("auth");
        prefs.remove("host");
      }
      loadDefault();
    } else {
      LOG_E("Config erase failed");
    }
  }

private:

  bool loadPrefs() {
    loadDefault();

    Preferences prefs;
    if (prefs.begin(BLYNK_PREFS_NAMESPACE, true)) { // read-only
      _cfgskip = prefs.getString("cfgskip", "0").toInt();
      _fwver = prefs.getString("fwver");
      _auth  = prefs.getString("auth",  _auth);
      _host  = prefs.getString("host",  _host);
      _saved = (_auth.length() == 32);
      return _saved;
    }
    LOG_W("No configuration is loaded");
    return false;
  }

private:
  bool          _saved;

  int           _cfgskip;
  String        _fwver;
  String        _auth;
  String        _host;
};
