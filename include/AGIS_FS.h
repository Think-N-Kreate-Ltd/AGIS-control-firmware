#ifndef B14E07C5_5834_48DE_94E2_DBA019FA913A
#define B14E07C5_5834_48DE_94E2_DBA019FA913A

#include <LittleFS.h>

void createDir(fs::FS &fs, const char * path);
String readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);

#endif /* B14E07C5_5834_48DE_94E2_DBA019FA913A */
