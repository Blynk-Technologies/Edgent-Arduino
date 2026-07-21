/**
 * @author     Volodymyr Shymanskyy
 * @copyright  Copyright (c) 2023 Volodymyr Shymanskyy
 */

#ifndef NetMgrUtils_h
#define NetMgrUtils_h

#include <NetMgrLogger.h>

#if defined(ESP32) || defined(ESP8266)
  #include <lwip/def.h>
#endif

#if defined(htons)
  #define nm_hton16(x) htons(x)
  #define nm_hton32(x) htonl(x)
  #define nm_ntoh16(x) ntohs(x)
  #define nm_ntoh32(x) ntohl(x)
#elif (defined(ARDUINO) || defined(PARTICLE) || defined(__MBED__))
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define nm_hton16(x) ( ((x)<<8) | (((x)>>8)&0xFF) )
    #define nm_hton32(x) ( ((x)<<24 & 0xFF000000UL) | \
                           ((x)<< 8 & 0x00FF0000UL) | \
                           ((x)>> 8 & 0x0000FF00UL) | \
                           ((x)>>24 & 0x000000FFUL) )
    #define nm_ntoh16(x) nm_hton16(x)
    #define nm_ntoh32(x) nm_hton32(x)
  #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define nm_hton16(x) (x)
    #define nm_hton32(x) (x)
    #define nm_ntoh16(x) (x)
    #define nm_ntoh32(x) (x)
  #else
    #error "Byte order not defined"
  #endif
#endif

static inline
String macToString(const byte mac[6]) {
  char buff[20];
  snprintf(buff, sizeof(buff), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buff);
}

static inline
bool macIsValid(const byte* mac) {
  for (int i = 0; i < 6; i++) {
      if (mac[i]) return true;
  }
  return false;
}

static inline
String ipToString(IPAddress ip) {
  char buff[20];
  snprintf(buff, sizeof(buff), "%d.%d.%d.%d",
           ip[0], ip[1], ip[2], ip[3]);
  return String(buff);
}

static inline
bool ipIsValid(IPAddress ip) {
  return ip[0] || ip[1] || ip[2] || ip[3];
}

class NetMgrBufferWriter {
public:
    NetMgrBufferWriter(uint8_t* buff, size_t size)
        : _ptr(buff), _beg(buff), _end(buff+size)
    {}

    size_t writeUInt8(uint8_t val) {
        return write(&val, sizeof(val));
    }

    size_t writeUInt16(uint16_t val) {
        uint16_t buff = nm_hton16(val);
        return write(&buff, sizeof(buff));
    }

    size_t write(const void* buff, size_t len) {
        if (_ptr + len <= _end) {
            memcpy(_ptr, buff, len);
            _ptr += len;
            return len;
        }
        return 0;
    }

    size_t write(const String& s) {
        size_t len = s.length();
        if (_ptr + len + 1 <= _end) {
            *_ptr++ = len;
            return write((const uint8_t*)s.c_str(), len)+1;
        }
        return 0;
    }

    size_t write(IPAddress ip) {
        uint8_t buff[4] = { ip[0], ip[1], ip[2], ip[3] };
        return write(buff, sizeof(buff));
    }

    size_t getOffset() const {
        return (_ptr - _beg);
    }

private:
    uint8_t *_ptr, *const _beg, *const _end;
};

class NetMgrBufferReader {
public:
    NetMgrBufferReader(const uint8_t* buff, size_t size)
        : _ptr(buff), _beg(buff), _end(buff+size)
    {}

    size_t readUInt8(uint8_t& val) {
        return read(&val, sizeof(val));
    }

    size_t readUInt16(uint16_t& val) {
        size_t res = read(&val, sizeof(val));
        if (res) { val = nm_ntoh16(val); }
        return res;
    }

    size_t read(void* buff, size_t len) {
        if (_ptr + len <= _end) {
            memcpy(buff, _ptr, len);
            _ptr += len;
            return len;
        }
        return 0;
    }

    size_t read(String& s) {
        if (_ptr + 1 <= _end) {
            size_t len = *_ptr++;
            if (_ptr + len <= _end) {
#if defined(ESP8266) || defined(ARDUINO_ARCH_NRF5) || defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_AMEBA)
                char buff[64];
                if (len >= sizeof(buff)) {
                    len = sizeof(buff) - 1;
                }
                memcpy(buff, _ptr, len);
                buff[len] = '\0';
                s = buff;
#else
                s = String((const char*)_ptr, len);
#endif
                _ptr += len;
                return len + 1;
            }
        }
        return 0;
    }

    size_t read(IPAddress& ip) {
        uint8_t buff[4];
        size_t res = read(buff, sizeof(buff));
        if (res) {
            ip = IPAddress(buff);
        }
        return res;
    }

    size_t getOffset() const {
        return (_ptr - _beg);
    }

private:
    const uint8_t *_ptr, *const _beg, *const _end;
};

#endif /* NetMgrUtils_h */
