#include <AGIS_SD.h>
#include <AGIS_Commons.h>
#include <AGIS_INA219.h>

// File system object.
SdFat sd;

// Log file.
SdFile file;

char today[4];     // var for storing the weekday of today
char datetime[11]; // var for storing the date time
char fileName[32]; // var for storing the path of file
                   // assume VBTI=5char, time=5char, total should be 30char.

void useSdCard(bool state) {
  // one SPI can only communicate with one device at the same time
  // in most cases, the CS pin goes to LOW only when using
  if (state) {
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, LOW);
  } else {
    digitalWrite(TFT_CS, LOW);
    digitalWrite(SD_CS, HIGH);
  }
}

void getTime() {  // use for getting the real time
  // config time logging with NTP server
  configTime(28800, 0, "pool.ntp.org");  // 60x60x8=28800, +8h for Hong Kong

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    ESP_LOGE(DATA_LOGGING_TAG, "Failed to obtain time");
    // return;
  }

  strftime(today , 4, "%a", &timeinfo);            // get the weekday of today
  strftime(datetime , 11, "%a/%H%M%S", &timeinfo); // get the first base name
}

void rmOldData() {  // remove data that is a week before
  getTime();

  char path[5];
  char weekday [7][4]= {{"Sun"}, {"Mon"}, {"Tue"}, {"Wed"}, {"Thu"}, {"Fri"}, {"Sat"}};
  for (int x=0; x<7; x++) {
    if (strcmp(weekday[x], today) == 0){
      if (x<6) {
        strcpy(path, "/");
        strcat(path, weekday[x+1]); // get the next weekday
      } else {
        strcpy(path, "/");
        strcat(path, weekday[0]);   // get Sunday
      }
    }
  }

  // Remove old data (a week before)
  if (!file.open(path)) {
    sd.errorHalt("file.open");
  }
  if (!file.rmRfStar()) { // remove all contents of the directory, also itself
    sd.errorHalt("rmdir failed.");
  } else {
    ESP_LOGI(DATA_LOGGING_TAG, "Old directory removed");
  }
  if (!sd.mkdir(path)) {
    sd.errorHalt("file.mkdir");
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
  getTime();
  sprintf(fileName, "%s_%u_%u_%u.csv", datetime, targetVTBI, targetTotalTime, dropFactor);

  // change the file name if already exists
  while (sd.exists(fileName)) {
    // seems not possible to happened
    sprintf(fileName, "%s_%u_%u_%u_2.csv", datetime, targetVTBI, targetTotalTime, dropFactor);
  }
  if (!file.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
    sd.errorHalt("file.open");
  }

  file.print(F("Time, Drip Rate, Infused Volume, Current, Bus Voltage, Shunt Voltage, Power, Average Current"));
  file.println();
}

void logData() {
  // // to avoid SD write latency between readings
  // float data[8] = {infusedTime, dripRate, infusedVolume_x100 / 100.0f, current_mA, busvoltage, shuntvoltage,
  //                     power_mW, avgCurrent_mA};

  // // Write the first data to CSV record
  // file.print(data[0]);

  // // Write data to CSV record
  // for (uint8_t i = 1; i < 8; i++) {
  //   file.write(',');
  //   // if (i==2) { // print the third colume with 1 d.p.
  //   //   uint16_t x = infusedVolume_x100 / 100;
  //   //   file.printf("%d.%d", x, infusedVolume_x100 / 10 - (x*10));
  //   // } else {
  //     file.print(data[i]);
  //   // }
  // }
  // file.println();

  file.printf("%u, %u, %f, %f, %f, %f, %f, %.1f\n", infusedTime, dripRate, 
                  infusedVolume_x100 / 100.0f, current_mA, busvoltage, 
                  shuntvoltage, power_mW, avgCurrent_mA);

  // Force data to SD and update the directory entry to avoid data loss.
  if (!file.sync() || file.getWriteError()) {
    sd.errorHalt("write error");
  }
}

void endLogging() {
  // Close file and stop.
  file.close();
  ESP_LOGI(DATA_LOGGING_TAG, "Data logging done!");
}

void loadFromSdCard(AsyncWebServerRequest *& request) {
  if (!file.open(fileName, O_RDONLY)) { // open for read only
    sd.errorHalt("file.open");
  }

  unsigned int dataAvaliable = file.fileSize() - file.curPosition();

  AsyncWebServerResponse *response = request->beginResponse("text/plain", dataAvaliable,
    [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    uint32_t readBytes;
    uint32_t bytes = 0;
    uint32_t avaliableBytes = file.available();

    if (avaliableBytes > maxLen) {
      bytes = file.read(buffer, maxLen);
    }
    else {
      bytes = file.read(buffer, avaliableBytes);
      file.close();
    }
    return bytes;
  });

  response->addHeader("Cache-Control", "no-cache");
  response->addHeader("Content-Disposition", "attachment; filename=" + String(fileName));
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
} 