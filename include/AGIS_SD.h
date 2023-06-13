#ifndef AGIS_SD_H
#define AGIS_SD_H

#include <SPI.h>
#include "SdFat.h"

#define SD_MISO 21
#define SD_MOSI 11
#define SD_SCK  12
#define SD_CS   9
#define TFT_CS  10

void rmOldData();
void sdCardSetUp();
void newFileInit();
void logData();
void endLogging();
void changeSpiDevice();

#endif /* AGIS_SD_H */