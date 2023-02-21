
#define DEBUG_FILES
#if defined(DEBUG_FILES)
  #undef LOG_DBG
  #define LOG_DBG(format, ...) Serial.printf(DBG_FORMAT(format,"DBG"), ##__VA_ARGS__)
#endif

#define MIN_STORAGE_SPACE (64 * 1024)  //Minimum allowed space for log rotate to work
#define DATA_DIR "/data"                //Dicectory to save data logs

// Append text at end of a file
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
  file.close();
}
// List a directory
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
//List dir and files sorded by name disc
bool listSortedDir(String dirName, std::vector<String> &dirArr, std::vector<std::vector<String>> &fileArr ) {
  LOG_INF("Listing directory: %s\n", dirName.c_str());
  File root = STORAGE.open(dirName.c_str());
  if (!root) {
    LOG_ERR("Failed to open dir: %s\n", dirName.c_str());
    return "";
  }
  /*
  if (!root.isDirectory()) {
    LOG_ERR("Not a directory\n");
    return false;
  }*/

  File file = root.openNextFile();
  String oPath = "";
  while (file) {
    String filePath = file.path();
    String dirPath = filePath.substring( 0, filePath.lastIndexOf("/") );
    String fileName = filePath.substring( filePath.lastIndexOf("/") + 1);

    if(oPath != dirPath ){
      oPath = dirPath;
      dirArr.push_back(dirPath);
    }
    String subDir = filePath.substring( 0, filePath.lastIndexOf("/") );
    LOG_DBG("List Dir: %s, sub Dir: %s  file: %s, sz: %lu\n", 
      dirPath.c_str(), subDir.c_str(), fileName.c_str(), file.size());
    
    if(dirName == subDir)
      fileArr.push_back( {fileName, String(float(file.size()/1024),0)} );

    file = root.openNextFile();
  }
  std::sort(dirArr.begin(), dirArr.end(), [] (
    const String &a, const String &b) {
    return a > b;}
  );  
  dirArr.erase(std::unique(dirArr.begin(), dirArr.end()), dirArr.end());

  std::sort(fileArr.begin(), fileArr.end(), [] (
    const std::vector<String> &a, const std::vector<String> &b) {
    return a[0] > b[0];}
  );
  return true;
}

// Get the oldest dir inside a folder
String getOldestDir(String dirName){

 File root = STORAGE.open(dirName.c_str());
  if (!root) {
    LOG_ERR("Failed to open dir: %s\n", dirName.c_str());
    return "";
  }
  
  File file = root.openNextFile();
  std::vector<String> dirArr;

  String oPath = "";
  while (file) {
    String filePath = file.path();
    String dirPath = filePath.substring( 0, filePath.lastIndexOf("/") );
    String fileName = filePath.substring( filePath.lastIndexOf("/") + 1);

    if(oPath != dirPath ){
      oPath = dirPath;
      LOG_DBG("dir: %s, file: %s\n", dirPath.c_str(), fileName.c_str());
      dirArr.push_back(dirPath);
    }   
    file = root.openNextFile();
  }
  std::sort(dirArr.begin(), dirArr.end(), [] (
    const String &a, const String &b) {
    return a < b;}
  );

  return (dirArr.size()>0 ? dirArr[0] : "");
}
// Is spiffs dir
bool isDirectory(String d) { return d.indexOf("/") >=0; }

// Remove a folder from spiffs
void removeFolder(String dirName){
  File root = STORAGE.open(dirName.c_str());
  if (!root) {
    LOG_ERR("Failed to open dir: %s\n", dirName.c_str());
    return;
  }
  
  File file = root.openNextFile();
  while (file) {
    String filePath = file.path();
    file = root.openNextFile();
    LOG_INF("Removing file: %s \n", filePath.c_str());
    /*
    if(!STORAGE.remove(filePath.c_str())){
      LOG_ERR("Remove FAILED: %s \n", filePath.c_str());
    };*/
  }
  LOG_INF("Removing dir: %s \n", dirName.c_str());
  //STORAGE.rmdir(dirName);
}

// Check an delete logs to save space
void checkLogRotate(){
  size_t free = STORAGE.totalBytes() - STORAGE.usedBytes();
  LOG_DBG("Storage, free: %lu K\n", (free/1024));
  if(free < MIN_STORAGE_SPACE){
    LOG_WRN("Storage is running low, free: %lu\n", free);
    String oldestDir = getOldestDir(DATA_DIR);    
    removeFolder(oldestDir);
  }
}