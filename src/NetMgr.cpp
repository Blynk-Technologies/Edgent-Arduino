/**
 * @author     Volodymyr Shymanskyy
 * @copyright  Copyright (c) 2026 Volodymyr Shymanskyy
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "NetMgr.h"

NetworkManagerClass NetMgr;

#if defined(NetMgr_WiFi)
  NetMgrWiFiClass NetMgrWiFi;
#endif

#if defined(NetMgr_Ethernet)
  NetMgrEthernetClass NetMgrEthernet;
#endif

#if defined(NetMgr_Cellular)
  NetMgrCellularClass NetMgrCellular;
#endif

#ifndef ARDUINO_ISR_ATTR
#define ARDUINO_ISR_ATTR
#endif

ARDUINO_ISR_ATTR
const char* logPath2Fn(const char* path)
{
    size_t i = 0;
    size_t pos = 0;
    char * p = (char *)path;
    while (*p) {
        i++;
        if (*p == '/' || *p == '\\') {
            pos = i;
        }
        p++;
    }
    return path + pos;
}

#if defined(LOGGER_PRINT) && defined(ARDUINO)
#include <stdarg.h>

void logPrintf(Stream& stream, const char *fmt, ... ) {
    static char buf[256];
    va_list args;
    va_start (args, fmt);
    vsnprintf(buf, sizeof(buf), (char*)fmt, args);
    va_end (args);
    stream.print(buf);
}

#endif

