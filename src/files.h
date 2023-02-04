
#define DEBUG_FILES
#if defined(DEBUG_FILES)
  #undef LOG_DBG
  #define LOG_DBG(format, ...) Serial.printf(DBG_FORMAT(format), ##__VA_ARGS__)
#endif

#define ONEMEG (1024 * 1024)
#define DATA_DIR "/data" //Dicectory to save data logs
String readString; // do not change this variable

//Append text at end of a file
void writeFile(const char * path, const char * message) {
  File file = STORAGE.open(path, FILE_APPEND);
  if (!file) {
    LOG_ERR("\nFailed to open: %s\n", path);
    return;
  }
  if (file.print(message)) {
    LOG_DBG("Writing file: %s OK\n", path);
  } else {
    LOG_ERR("\nFailed to write: %s\n", path);
  }
}

void listDir(const char * dirname, uint8_t levels) {
  LOG_INF("Listing directory: %s\n", dirname);

  File root = STORAGE.open(dirname);
  if (!root) {
    LOG_ERR("- failed to open directory\n");
    return;
  }
  if (!root.isDirectory()) {
    LOG_ERR(" - not a directory\n");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      LOG_INF("  Dir: %s\n", file.name());
      if (levels) {
        listDir(file.name(), levels - 1);
      }
    } else {
      LOG_INF("  File: %s, sz: %lu\n", file.path(), file.size());
    }
    file = root.openNextFile();
  }
}
