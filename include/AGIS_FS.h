#ifndef B14E07C5_5834_48DE_94E2_DBA019FA913A
#define B14E07C5_5834_48DE_94E2_DBA019FA913A

#include <LittleFS.h>
#include "FS.h"

/*----------function that example provided (decrecated)----------*/
void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
void readFile(fs::FS &fs, const char * path);
void writeFile(fs::FS &fs, const char * path, const char * message);
void writeFile2(fs::FS &fs, const char * path, const char * message);

/*-------------------function that self added-------------------*/
void readDF(fs::FS &fs, const char * path);

/*------------function only use for fixing FS problem------------*/
void deleteRfStar(fs::FS &fs, const char * dirname, uint8_t levels=3);

#endif /* B14E07C5_5834_48DE_94E2_DBA019FA913A */
