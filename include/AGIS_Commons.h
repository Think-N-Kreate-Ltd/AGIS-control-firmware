#ifndef D8C84950_1B28_4594_B461_3D52C4A78A6B
#define D8C84950_1B28_4594_B461_3D52C4A78A6B

#include <AGIS_Types.h>

extern volatile unsigned int numDrops;
extern volatile unsigned int dripRate;
extern volatile unsigned int infusedVolume_x100;
extern volatile unsigned int infusedTime;
extern unsigned int dropFactor;

extern unsigned int targetVTBI;
extern unsigned int targetTotalTime;
extern unsigned int targetDripRate;
extern unsigned int targetNumDrops;
extern infusionState_t infusionState;

static const char* DATA_LOGGING_TAG = "DATA_LOGGING";
static const char* KEYPAD_TAG = "KEYPAD";
static const char* WEBSOCKET_TAG = "WEBSOCKET";
static const char* LITTLE_FS_TAG = "LITTLE_FS";
static const char* WIFI_TAG = "WIFI";
static const char* OLED_TAG = "OLED";

// extern char *logFilePath;
// extern const char* ntpServer;
// extern const long gmtOffset_sec;
// extern const int daylightOffset_sec;
extern bool enableLogging;
// extern bool loggingCompleted;

#endif /* D8C84950_1B28_4594_B461_3D52C4A78A6B */
