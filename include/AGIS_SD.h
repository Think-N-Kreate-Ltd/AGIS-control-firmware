#ifndef AGIS_SD_H
#define AGIS_SD_H

#include <SPI.h>
#include "SdFat.h"

#define SD_MISO 21
#define SD_MOSI 11
#define SD_SCK  12
#define SD_CS   9
#define TFT_CS  10

void changeSpiDevice();
void getTime();
void rmOldData();

void sdCardSetUp();
void newFileInit();
void logData();
void endLogging();

// File system object.
extern SdFat sd;

// Log file.
extern SdFile file;

extern char datetime[11]; // var for storing the date time
extern char fileName[32]; // var for storing the path of file

#endif /* AGIS_SD_H */