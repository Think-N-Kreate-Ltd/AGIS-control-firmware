#include <AGIS_SD.h>
#include <AGIS_Commons.h>

// Log file base name.  Must be six characters or less.
#define FILE_BASE_NAME "Sat/"

char datetime[4]; // var for storing the weekdays

// File system object.
SdFat sd;

// Log file.
SdFile file;

// Error messages stored in flash.
#define error(msg) sd.errorHalt(F(msg))

const uint8_t ANALOG_COUNT = 3;

void rmOldData() {
    // use for getting the real time
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    ESP_LOGE(DATA_LOGGING_TAG, "Failed to obtain time");
    // return;
  }

  char path[5];
  char weekday [7][4]= {{"Sun"}, {"Mon"}, {"Tue"}, {"Wed"}, {"Thu"}, {"Fri"}, {"Sat"}};
  strftime(datetime , 4, "%a", &timeinfo); // get the first base name
  for (int x=0; x<7; x++) {
    if (strcmp(weekday[x], datetime) == 0){
      if (x<6) {
        strcpy(path, "/");
        strcat(path, weekday[x+1]); // get the previous day
      } else {
        strcpy(path, "/");
        strcat(path, weekday[0]);   // get Sunday
      }
    }
  }

  // Remove old data (a week before)
  if (!file.open(path)) {
    error("file.open");
  }
  if (!file.rmRfStar()) { // remove all contents of the directory, also itself
    sd.errorHalt("rmdir failed.");
  } else {
    ESP_LOGI(DATA_LOGGING_TAG, "Old directory removed");
  }
  if (!sd.mkdir(path)) {
    error("file.mkdir");
  }
  file.close();
}

void sdCardSetUp() {
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, TFT_CS);

  // Initialize at the highest speed supported by the board that is
  // not over 50 MHz. Try a lower speed if SPI errors occur.
  if (!sd.begin(SD_CS, SD_SCK_MHZ(40))) {
    sd.initErrorHalt();
  }
}

void newFileInit() {
  const uint8_t BASE_NAME_SIZE = 4; // the base name should not >6
  char fileName[13];
  strcpy(fileName, datetime);
  strcat(fileName, "/");
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

void endLogging() {
  // Close file and stop.
  file.close();
  ESP_LOGI(DATA_LOGGING_TAG, "Data loggong done!");
  while (true) {}
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