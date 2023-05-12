#include <AGIS_Logging.h>
#include <time.h>
#include <LittleFS.h>
#include <AGIS_Commons.h>

// for data logging
char *logFilePath;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800;  // for Hong Kong
const int daylightOffset_sec = 0;
bool loggingCompleted = false;
bool enableLogging = false;

// NOTE: do not call this function inside interrupt,
// since getLocalTime() causes panic
void logInit() {
  // logFilePath format: datetime_volume_time_dropfactor
  // e.g. 2023April21084024_100_3600_20.csv

  // get date and time from NTP server
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    ESP_LOGE(DATA_LOGGING_TAG, "Failed to obtain time");
    if (asprintf(&logFilePath, "default.csv")) {
      loggingCompleted = false;
    } else {
      ESP_LOGE(DATA_LOGGING_TAG, "Error when creating logFilePath");
    }
  }

  // char datetime[30];
  // strftime(datetime,30, "%Y%B%d%H%M%S", &timeinfo);

  // NOTE: SPIFFS maximum logFilePath is 32 characters
  // only use H:M:S to save characters
  char datetime[9];
  strftime(datetime,9, "%H%M%S", &timeinfo);

  if (asprintf(&logFilePath, "/%s_%u_%u_%u.csv", datetime, targetVTBI,
               targetTotalTime, dropFactor)) {
    loggingCompleted = false;
    ESP_LOGI(DATA_LOGGING_TAG, "logFilePath created successfully as: %s", logFilePath);
  } else {
    ESP_LOGE(DATA_LOGGING_TAG, "Error when creating logFilePath");
  }
}

// Return value:
//    true when logging is completed
//    false when logging is still in progress
bool logInfusionMonitoringData(char* logFilePath) {
  // write csv header
  if (!LittleFS.exists(logFilePath)) {
    ESP_LOGI(DATA_LOGGING_TAG, "Logging started...");
    File file = LittleFS.open(logFilePath, FILE_WRITE);
    if (!file) {
      ESP_LOGE(DATA_LOGGING_TAG, "There was an error opening the file for writing");
      return false;
    }

    if (file.printf("%s, %s, %s\n", "Time", "Drip Rate", "Infused Volume")) {
      ESP_LOGV(DATA_LOGGING_TAG, "Header write OK");
    }
    else {
      ESP_LOGE(DATA_LOGGING_TAG, "Header write failed");
    }
    file.close();
  }

  // TODO: use folder for all data files
  File file = LittleFS.open(logFilePath, FILE_APPEND);
  if (!file) {
    ESP_LOGE(DATA_LOGGING_TAG, "There was an error opening the file for writing");
    return false;
  }

  if(file.printf("%u, %u, %f\n", infusedTime, dripRate, infusedVolume_x100 / 100.0f)) {
    ESP_LOGV(DATA_LOGGING_TAG, "File was written");
  }else {
    ESP_LOGE(DATA_LOGGING_TAG, "File write failed");
  }
  file.close();

  // check if we can end logging
  if (infusionState == infusionState_t::ALARM_COMPLETED) {
    ESP_LOGI(DATA_LOGGING_TAG, "Data logging completed, filename: %s", logFilePath);
    return true;
  }
  else return false;
}