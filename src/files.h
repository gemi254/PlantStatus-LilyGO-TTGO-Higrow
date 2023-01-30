#define ONEMEG (1024 * 1024)
#define DATA_DIR "/data" //Dicectory to save data logs
String readString; // do not change this variable

//Append text at end of a file
void writeFile(const char * path, const char * message) {
  LOG_DBG("Writing file: %s", path);

  File file = STORAGE.open(path, FILE_APPEND);
  if (!file) {
    LOG_ERR("\nFailed to open: %s", path);
    return;
  }
  if (file.print(message)) {
    LOG_DBG(" OK\n");
  } else {
    LOG_ERR("\nFailed to write: %s\n", path);
  }
}

void listDir(const char * dirname, uint8_t levels) {
  LOG_INF("Listing directory: %s\r\n", dirname);

  File root = STORAGE.open(dirname);
  if (!root) {
    LOG_ERR("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    LOG_ERR(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      LOG_INF("  DIR : ");
      LOG_INF("%s" ,file.name());
      if (levels) {
        listDir(file.name(), levels - 1);
      }
    } else {
      LOG_INF("  FILE: ");
      LOG_INF("%s" ,file.name());
      LOG_INF("\tSIZE: ");
      LOG_INF("%lu",file.size());
    }
    file = root.openNextFile();
  }
}
