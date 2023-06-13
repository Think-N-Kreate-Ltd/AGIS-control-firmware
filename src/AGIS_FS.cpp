#include <AGIS_FS.h>
#include <AGIS_Commons.h>

void createDir(fs::FS &fs, const char * path){
  if (!LittleFS.exists("/index.html")){
    ESP_LOGI(LITTLE_FS_TAG, "Creating Dir: %s", path);
    if(fs.mkdir(path)){
      ESP_LOGI(LITTLE_FS_TAG, "Dir created");
    } else {
      ESP_LOGE(LITTLE_FS_TAG, "mkdir failed");
    }
  } 
}

String readFile(fs::FS &fs, const char *path) {
  ESP_LOGI(LITTLE_FS_TAG, "Reading file: %s", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    ESP_LOGE(LITTLE_FS_TAG, "- empty file or failed to open file");
    return String();
  }
  ESP_LOGI(LITTLE_FS_TAG, "- read from file: ");
  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  file.close();
  ESP_LOGI(LITTLE_FS_TAG, "%s", fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  ESP_LOGI(LITTLE_FS_TAG, "Writing file: %s", path);
  File file = fs.open(path, "w");
  if (!file) {
    ESP_LOGE(LITTLE_FS_TAG, "- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    ESP_LOGI(LITTLE_FS_TAG, "- file written");
  } else {
    ESP_LOGE(LITTLE_FS_TAG, "- write failed");
  }
  file.close();
}