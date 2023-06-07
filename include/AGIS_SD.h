#ifndef AGIS_SD_H
#define AGIS_SD_H

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define SD_MISO 21
#define SD_MOSI 11
#define SD_SCK  12
#define SD_CS   9
#define TFT_CS  10

void sdCardSetUp();
void changeSpiDevice();

#endif /* AGIS_SD_H */