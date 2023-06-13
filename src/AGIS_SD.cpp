#include <AGIS_SD.h>
#include <AGIS_Commons.h>

// Log file base name.  Must be six characters or less.
#define FILE_BASE_NAME "Sat/"

// File system object.
SdFat sd;

// Log file.
SdFile file;

// Error messages stored in flash.
#define error(msg) sd.errorHalt(F(msg))

const uint8_t ANALOG_COUNT = 3;

void newFileInit() {
  // use for getting the real time
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    ESP_LOGE(DATA_LOGGING_TAG, "Failed to obtain time");
    // return;
  }
  char datetime[4];
  strftime(datetime ,13, "%a/", &timeinfo); // get the first base name

  const uint8_t BASE_NAME_SIZE = 4; // the base name should not >6
  char fileName[13];
  strcpy(fileName, datetime);
  strcat(fileName, "00.csv");

  // get the next number for file name
  while (sd.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
      error("Can't create file name");
    }
  }
  if (!file.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
    error("file.open");
  }

  file.print(F("Time, Drip Rate, Infused Volume"));
  file.println();
}

void logData() {
  // to avoid SD write latency between readings
  uint16_t data[3] = {infusedTime, dripRate, infusedVolume_x100};

  // Write the first data to CSV record
  file.print(data[0]);

  // Write data to CSV record
  for (uint8_t i = 1; i < 3; i++) {
    file.write(',');
    if (i==2) { // print the third colume with 1 d.p.
      uint16_t x = infusedVolume_x100 / 100;
      file.printf("%d.%d", x, infusedVolume_x100 / 10 - (x*10));
    } else {
      file.print(data[i]);
    }
  }
  file.println();

  // Force data to SD and update the directory entry to avoid data loss.
  if (!file.sync() || file.getWriteError()) {
    error("write error");
  }
}

void sdCardSetUp() {
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, TFT_CS);

  // Initialize at the highest speed supported by the board that is
  // not over 50 MHz. Try a lower speed if SPI errors occur.
  if (!sd.begin(SD_CS, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }

  // TODO: remove old data

  // check for weekday
  // char today[4];
  // char path[5]; // store the path of the folder of the next weekday
  // char weekday [7][4]= {{"Sun"}, {"Mon"}, {"Tue"}, {"Wed"}, {"Thu"}, {"Fri"}, {"Sat"}};
  // strftime(today, 4, "%a", &timeinfo);
  // for (int x=0; x<7; x++) {
  //   delay(100); // wait a short time, otherwise the sd card cannnot mount
  //   if (strcmp(weekday[x], today) == 0){
  //     if (x<6) {
  //       strcpy(path, "/");
  //       strcat(path, weekday[x+1]); // get the next day
  //     } else {
  //       strcpy(path, "/");
  //       strcat(path, weekday[0]);   // get Sunday
  //     }
  //   }
  // }
  // Serial.print("The directory to be cleared is ");
  // Serial.println(path);

  // // remove all files in the directory
  // File root2 = SD.open(path);
  // char rm_path[99];
  // strcpy(rm_path, path);
  // strcat(rm_path, "/");
  // // rm(root2, rm_path);

  // while (true) {  // keep doing until the directory is empty
  //   File entry = root2.openNextFile();
  //   if (entry) {
  //     if (entry.isDirectory()) {
  //       Serial.println("Not expected to remove the folder");
  //     } else {
  //       strcat(rm_path, entry.name());
  //       if (SD.remove(rm_path)) {
  //         Serial.printf("Deleted %s\n");
  //       } else {
  //         Serial.printf("Failed to delete %s\n");
  //       }
  //     }
  //   } else {
  //     // Enter here when the folder is empty. Stop the loop
  //     break;
  //   }
  // }
  

  // remove and create dir <- remove can only be done when the directory is empty
  // prevent unwanted delete
  // if(SD.rmdir(path)){
  //   Serial.println("Dir removed");
  // } else {
  //   Serial.println("rmdir failed");
  // }
  // Serial.printf("Creating Dir: %s\n", path);
  // if(SD.mkdir(path)){
  //   Serial.println("Dir created");
  // } else {
  //   Serial.println("mkdir failed");
  // }
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

void endLogging() {
  // Close file and stop.
  file.close();
  ESP_LOGI(DATA_LOGGING_TAG, "Data loggong done!");
  while (true) {}
}