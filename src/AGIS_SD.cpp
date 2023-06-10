#include <AGIS_SD.h>

// Log file base name.  Must be six characters or less.
#define FILE_BASE_NAME "Data"

// File system object.
SdFat sd;

// Log file.
SdFile file;

// Error messages stored in flash.
#define error(msg) sd.errorHalt(F(msg))

const uint8_t ANALOG_COUNT = 4;

void writeHeader() {
  file.print(F("micros"));
  for (uint8_t i = 0; i < ANALOG_COUNT; i++) {
    file.print(F(",adc"));
    file.print(i, DEC);
  }
  file.println();
}

void logData() {
  uint16_t data[ANALOG_COUNT];

  // Read all channels to avoid SD write latency between readings.
  for (uint8_t i = 0; i < ANALOG_COUNT; i++) {
    data[i] = analogRead(i);
  }
  // Write data to file.  Start with log time in micros.
  file.print(logTime);

  // Write ADC data to CSV record.
  for (uint8_t i = 0; i < ANALOG_COUNT; i++) {
    file.write(',');
    file.print(data[i]);
  }
  file.println();
}

void sdCardSetUp() {
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, TFT_CS);

  const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
  char fileName[13] = FILE_BASE_NAME "00.csv";

  delay(1000);  // remove later

  // Initialize at the highest speed supported by the board that is
  // not over 50 MHz. Try a lower speed if SPI errors occur.
  if (!sd.begin(SD_CS, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }

  // use for getting the real time
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    // return;
  }

  // Find an unused file name.
  if (BASE_NAME_SIZE > 6) {
    error("FILE_BASE_NAME too long");
  }
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
  // Read any Serial data.
  do {
    delay(10);
  } while (Serial.available() && Serial.read() >= 0);

  Serial.print(F("Logging to: "));
  Serial.println(fileName);
  Serial.println(F("Type any character to stop"));

  // Write data header.
  writeHeader();

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

void looptest() {
  delay(1000);
  logData();

  // Force data to SD and update the directory entry to avoid data loss.
  if (!file.sync() || file.getWriteError()) {
    error("write error");
  }

  if (Serial.available()) {
    // Close file and stop.
    file.close();
    Serial.println(F("Done"));
    while (true) {}
  }
}