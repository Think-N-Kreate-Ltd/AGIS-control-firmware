#ifndef AGIS_SD_H
#define AGIS_SD_H

#include <SPI.h>
#include "SdFat.h"
#include <ESPAsyncWebServer.h>

#define SD_MISO 21
#define SD_MOSI 11
#define SD_SCK  12
#define SD_CS   9
#define TFT_CS  10

void useSdCard(bool state = true);
void rmOldData();   // remove data that is a week before

void sdCardSetUp();
void newFileInit();
void logData();
void endLogging();

// force download file
// as SdFat is not supported, here use chunked response
// use pass by response to reduce the data copied
void loadFromSdCard(AsyncWebServerRequest *& request);

#endif /* AGIS_SD_H */