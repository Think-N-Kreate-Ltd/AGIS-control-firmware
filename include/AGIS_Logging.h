#ifndef A1C4896B_56AC_4143_8399_314B2B770C3B
#define A1C4896B_56AC_4143_8399_314B2B770C3B

static const char* DATA_LOGGING_TAG = "DATA_LOGGING";
static const char* KEYPAD_TAG = "KEYPAD";
static const char* WEBSOCKET_TAG = "WEBSOCKET";
static const char* LITTLE_FS_TAG = "LITTLE_FS";
static const char* WIFI_TAG = "WIFI";
static const char* OLED_TAG = "OLED";

extern char *logFilePath;
extern const char* ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;
extern bool enableLogging;
extern bool loggingCompleted;

void logInit();
bool logInfusionMonitoringData(char *logFilePath);

#endif /* A1C4896B_56AC_4143_8399_314B2B770C3B */
