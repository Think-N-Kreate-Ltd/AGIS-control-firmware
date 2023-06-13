#ifndef AGIS_SD_H
#define AGIS_SD_H

#include <SPI.h>
#include "SdFat.h"

#define SD_MISO 21
#define SD_MOSI 11
#define SD_SCK  12
#define SD_CS   9
#define TFT_CS  10

static const char* DATA_LOGGING_TAG = "DATA_LOGGING";
static const char* KEYPAD_TAG = "KEYPAD";
static const char* WEBSOCKET_TAG = "WEBSOCKET";
static const char* LITTLE_FS_TAG = "LITTLE_FS";
static const char* WIFI_TAG = "WIFI";
static const char* OLED_TAG = "OLED";

// check them one by one later
extern char *logFilePath;
extern const char* ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;
extern bool enableLogging;
extern bool loggingCompleted;

// TODO: order it, also in .cpp file
void newFileInit();
void logData();
void sdCardSetUp();
void changeSpiDevice();
void endLogging();

#endif /* AGIS_SD_H */