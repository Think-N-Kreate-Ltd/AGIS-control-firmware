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

// File system object.
extern SdFat sd;

// Log file.
extern SdFile file;

extern char datetime[11]; // var for storing the date time
extern char fileName[32]; // var for storing the path of file
                          // assume VBTI=5char, time=5char, total should be 30char.

#endif /* AGIS_SD_H */