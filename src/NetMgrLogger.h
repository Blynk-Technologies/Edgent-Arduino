/**
 * @author     Volodymyr Shymanskyy
 * @copyright  Copyright (c) 2023 Volodymyr Shymanskyy
 */

#ifndef NetMgrLogger_h
#define NetMgrLogger_h

#define LOGGER_LEVEL_NONE       (0)
#define LOGGER_LEVEL_ERROR      (1)
#define LOGGER_LEVEL_WARN       (2)
#define LOGGER_LEVEL_INFO       (3)
#define LOGGER_LEVEL_DEBUG      (4)

//#define LOGGER_PRINT      Serial
//#define LOGGER_LOG_LEVEL  4

#if defined(LOGGER_LOG_LEVEL)
  // Ok, use it
#elif defined(ESP32) && defined(CORE_DEBUG_LEVEL)
  #define LOGGER_LOG_LEVEL CORE_DEBUG_LEVEL
#else
  #define LOGGER_LOG_LEVEL LOGGER_LEVEL_NONE
#endif

#if defined(LOGGER_PRINT)
  // Ok, use it
#elif defined(ESP8266) && defined(DEBUG_ESP_PORT)
  #define LOGGER_PRINT DEBUG_ESP_PORT
#endif

#if defined(LOGGER_PRINT) && defined(ARDUINO)
  const char* logPath2Fn(const char * path);
  void        logPrintf(Stream& stream, const char *fmt, ... );
  #if defined(LOGGER_LOG_LINES)
    #define LOG_FMT "[%s:%d] "
    #define LOG_FNL logPath2Fn(__FILE__), __LINE__
  #else
    #define LOG_FMT "[%s] "
    #define LOG_FNL logPath2Fn(__FILE__)
  #endif
  #define LOG_MFMT "[%s] "

  #ifndef LOGGER_TIME
    #define LOGGER_TIME millis()
  #endif
  #define LOG_DEFINE_MODULE(name) static const char* LOG_TAG = name;
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_ERROR
    #define LOG_E(fmt, ...)       do { logPrintf(LOGGER_PRINT, "[%6lu][E]" LOG_FMT,  LOGGER_TIME, LOG_FNL); logPrintf(LOGGER_PRINT, fmt "\n", ##__VA_ARGS__); } while(0)
    #define LOG_E_MOD(fmt, ...)   do { logPrintf(LOGGER_PRINT, "[%6lu][E]" LOG_MFMT, LOGGER_TIME, LOG_TAG); logPrintf(LOGGER_PRINT, fmt "\n", ##__VA_ARGS__); } while(0)
  #endif
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_WARN
    #define LOG_W(fmt, ...)       do { logPrintf(LOGGER_PRINT, "[%6lu][W]" LOG_FMT,  LOGGER_TIME, LOG_FNL); logPrintf(LOGGER_PRINT, fmt "\n", ##__VA_ARGS__); } while(0)
    #define LOG_W_MOD(fmt, ...)   do { logPrintf(LOGGER_PRINT, "[%6lu][W]" LOG_MFMT, LOGGER_TIME, LOG_TAG); logPrintf(LOGGER_PRINT, fmt "\n", ##__VA_ARGS__); } while(0)
  #endif
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_INFO
    #define LOG_I(fmt, ...)       do { logPrintf(LOGGER_PRINT, "[%6lu][I]" LOG_FMT,  LOGGER_TIME, LOG_FNL); logPrintf(LOGGER_PRINT, fmt "\n", ##__VA_ARGS__); } while(0)
    #define LOG_I_MOD(fmt, ...)   do { logPrintf(LOGGER_PRINT, "[%6lu][I]" LOG_MFMT, LOGGER_TIME, LOG_TAG); logPrintf(LOGGER_PRINT, fmt "\n", ##__VA_ARGS__); } while(0)
  #endif
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_DEBUG
    #define LOG_D(fmt, ...)       do { logPrintf(LOGGER_PRINT, "[%6lu][D]" LOG_FMT,  LOGGER_TIME, LOG_FNL); logPrintf(LOGGER_PRINT, fmt "\n", ##__VA_ARGS__); } while(0)
    #define LOG_D_MOD(fmt, ...)   do { logPrintf(LOGGER_PRINT, "[%6lu][D]" LOG_MFMT, LOGGER_TIME, LOG_TAG); logPrintf(LOGGER_PRINT, fmt "\n", ##__VA_ARGS__); } while(0)
  #endif
#elif defined(ESP32) && defined(ARDUINO)
  // NOTE: On Arduino, all ESP_LOGx are mapped to log_x anyway
  #define LOG_DEFINE_MODULE(name) static const char* LOG_TAG = name;
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_ERROR
    #define LOG_E(fmt, ...)       log_e(fmt, ##__VA_ARGS__)
    #define LOG_E_MOD(fmt, ...)   log_e("[%s] " fmt, LOG_TAG, ##__VA_ARGS__)
  #endif
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_WARN
    #define LOG_W(fmt, ...)       log_w(fmt, ##__VA_ARGS__)
    #define LOG_W_MOD(fmt, ...)   log_w("[%s] " fmt, LOG_TAG, ##__VA_ARGS__)
  #endif
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_INFO
    #define LOG_I(fmt, ...)       log_i(fmt, ##__VA_ARGS__)
    #define LOG_I_MOD(fmt, ...)   log_i("[%s] " fmt, LOG_TAG, ##__VA_ARGS__)
  #endif
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_DEBUG
    #define LOG_D(fmt, ...)       log_d(fmt, ##__VA_ARGS__)
    #define LOG_D_MOD(fmt, ...)   log_d("[%s] " fmt, LOG_TAG, ##__VA_ARGS__)
  #endif
#elif defined(ESP32)
  #define LOG_DEFINE_MODULE(name) static const char* LOG_TAG = name;
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_ERROR
    #define LOG_E(fmt, ...)       ESP_LOGE("app", fmt, ##__VA_ARGS__)
    #define LOG_E_MOD(fmt, ...)   ESP_LOGE(LOG_TAG, fmt, ##__VA_ARGS__)
  #endif
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_WARN
    #define LOG_W(fmt, ...)       ESP_LOGW("app", fmt, ##__VA_ARGS__)
    #define LOG_W_MOD(fmt, ...)   ESP_LOGW(LOG_TAG, fmt, ##__VA_ARGS__)
  #endif
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_INFO
    #define LOG_I(fmt, ...)       ESP_LOGI("app", fmt, ##__VA_ARGS__)
    #define LOG_I_MOD(fmt, ...)   ESP_LOGI(LOG_TAG, fmt, ##__VA_ARGS__)
  #endif
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_DEBUG
    #define LOG_D(fmt, ...)       ESP_LOGD("app", fmt, ##__VA_ARGS__)
    #define LOG_D_MOD(fmt, ...)   ESP_LOGD(LOG_TAG, fmt, ##__VA_ARGS__)
  #endif
#elif defined(PARTICLE)
  #define LOG_DEFINE_MODULE(name) static Logger local_logger(name);
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_ERROR
    #define LOG_E(fmt, ...)       Log.error(fmt, ##__VA_ARGS__)
    #define LOG_E_MOD(fmt, ...)   local_logger.error(fmt, ##__VA_ARGS__)
  #endif
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_WARN
    #define LOG_W(fmt, ...)       Log.warn( fmt, ##__VA_ARGS__)
    #define LOG_W_MOD(fmt, ...)   local_logger.warn( fmt, ##__VA_ARGS__)
  #endif
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_INFO
    #define LOG_I(fmt, ...)       Log.info( fmt, ##__VA_ARGS__)
    #define LOG_I_MOD(fmt, ...)   local_logger.info( fmt, ##__VA_ARGS__)
  #endif
  #if LOGGER_LOG_LEVEL >= LOGGER_LEVEL_DEBUG
    #define LOG_D(fmt, ...)       Log.trace(fmt, ##__VA_ARGS__)
    #define LOG_D_MOD(fmt, ...)   local_logger.trace(fmt, ##__VA_ARGS__)
  #endif
#endif

#ifndef LOG_DEFINE_MODULE
#define LOG_DEFINE_MODULE(name)
#endif

#ifndef LOG_E
#define LOG_E(fmt, ...)
#define LOG_E_MOD(fmt, ...)
#endif

#ifndef LOG_W
#define LOG_W(fmt, ...)
#define LOG_W_MOD(fmt, ...)
#endif

#ifndef LOG_I
#define LOG_I(fmt, ...)
#define LOG_I_MOD(fmt, ...)
#endif

#ifndef LOG_D
#define LOG_D(fmt, ...)
#define LOG_D_MOD(fmt, ...)
#endif

#endif /* NetMgrLogger_h */
