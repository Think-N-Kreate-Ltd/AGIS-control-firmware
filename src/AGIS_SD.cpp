#include <AGIS_SD.h>

// set up for SPI and SD card
SPIClass sd_spi = SPIClass(FSPI);

void sdCardSetUp() {
  sd_spi.begin(SD_SCK, SD_MISO, SD_MOSI, TFT_CS);

  // Initialize SD module
  if(!SD.begin(SD_CS, sd_spi)){
    Serial.println("Card Mount Failed");
    return;
  }

  // pinMode(SD_SCK, OUTPUT);
  // pinMode(SD_MOSI, OUTPUT);

  // use for getting the real time
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    // return;
  }

  // list the directory of the sd card (from web_server)
  Serial.printf("Listing directory: %s\n", "/web_server");
  File root = SD.open("/web_server");
  if(!root){
    Serial.println("Failed to open /web_server");
    return;
  }
  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }

  // check for weekday
  char today[4];
  char path[5]; // store the path of the folder of the next weekday
  char weekday [7][4]= {{"Sun"}, {"Mon"}, {"Tue"}, {"Wed"}, {"Thu"}, {"Fri"}, {"Sat"}};
  strftime(today, 4, "%a", &timeinfo);
  for (int x=0; x<7; x++) {
    delay(100); // wait a short time, otherwise the sd card cannnot mount
    if (strcmp(weekday[x], today) == 0){
      if (x<6) {
        strcpy(path, "/");
        strcat(path, weekday[x+1]); // get the next day
      } else {
        strcpy(path, "/");
        strcat(path, weekday[0]);   // get Sunday
      }
    }
  }
  Serial.print("The directory to be cleared is ");
  Serial.println(path);

  // remove all files in the directory
  File root2 = SD.open(path);
  char rm_path[99];
  strcpy(rm_path, path);
  strcat(rm_path, "/");
  // rm(root2, rm_path);

  while (true) {  // keep doing until the directory is empty
    File entry = root2.openNextFile();
    if (entry) {
      if (entry.isDirectory()) {
        Serial.println("Not expected to remove the folder");
      } else {
        strcat(rm_path, entry.name());
        if (SD.remove(rm_path)) {
          Serial.printf("Deleted %s\n");
        } else {
          Serial.printf("Failed to delete %s\n");
        }
      }
    } else {
      // Enter here when the folder is empty. Stop the loop
      break;
    }
  }
  

  // remove and create dir <- remove can only be done when the directory is empty
  // prevent unwanted delete
  if(SD.rmdir(path)){
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
  Serial.printf("Creating Dir: %s\n", path);
  if(SD.mkdir(path)){
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void changeSpiDevice() {
  // one SPI can only communicate with one device at the same time
  // in most cases, the CS pin goes to LOW only when using
  static bool state = true;
  if (state) {
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, LOW);
  } else {
    digitalWrite(TFT_CS, LOW);
    digitalWrite(SD_CS, HIGH);
  }
  state = !state;
}