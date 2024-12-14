
#define MIN_STORAGE_SPACE (50 * 1024)   // Minimum allowed space for log rotate to work
#define DATA_DIR "/data"                // Dicectory to save data logs

// Append text at end of a file
void writeFile(const char * path, const char * message) {
  #ifdef CA_USE_LITTLEFS
  File file = STORAGE.open(path, FILE_APPEND, true);
  #else
  File file = STORAGE.open(path, FILE_APPEND);
  #endif
  if (!file) {
    LOG_E("\nFailed to open: %s\n", path);
    return;
  }
  if (file.print(message)) {
    LOG_D("Writing file: %s OK\n", path);
  } else {
    LOG_E("\nFailed to write: %s\n", path);
  }
  file.close();
}

// Checks whether a given path corresponds to a directory
bool isDirectory(const String &path) {
  File file = STORAGE.open(path, "r");
  bool isDir = file && file.isDirectory();
  file.close();
  return isDir;
}

// List dir and files sorded by name disc
bool listSortedDir(const String &dirName, std::vector<String> &dirArr, std::vector<std::vector<String>> &fileArr ) {
  LOG_I("Listing directory: %s\n", dirName.c_str());
  File root = STORAGE.open(dirName.c_str());
  if (!root) {
    LOG_E("Failed to open dir: %s\n", dirName.c_str());
    return "";
  }

  File file = root.openNextFile();
  String oPath = "";
  while (file) {
    String filePath = file.path();
    LOG_D("List file: %s\n",filePath.c_str());
    if(isDirectory(filePath)){
      if(oPath != filePath ){
        oPath = filePath;
        dirArr.push_back(filePath);
      }
    }else{
      fileArr.push_back( {filePath, String(float(file.size()/1024),0)} );
    }
    file = root.openNextFile();
  }

  std::sort(dirArr.begin(), dirArr.end(), [] (
    const String &a, const String &b) {
    return a > b;}
  );
  dirArr.erase(std::unique(dirArr.begin(), dirArr.end()), dirArr.end());
  // Add root dir
  dirArr.push_back(dirName);

  std::sort(fileArr.begin(), fileArr.end(), [] (
    const std::vector<String> &a, const std::vector<String> &b) {
    return a[0] > b[0];}
  );
  return true;
}

// Get the oldest dir inside a folder
String getOldestDir(const String &dirName){
  std::vector<String> dirArr;               // Directory array
  std::vector<std::vector<String>> fileArr; // Files array

  listSortedDir(dirName, dirArr, fileArr );
  return (dirArr.size()>1 ? dirArr[dirArr.size() -2] : "");
}

// Remove a folder from spiffs
void removeFolder(String dirName){
  File root = STORAGE.open(dirName.c_str());
  if (!root) {
    LOG_E("Failed to open dir: %s\n", dirName.c_str());
    return;
  }

  File file = root.openNextFile();
  while (file) {
    String filePath = file.path();
    file = root.openNextFile();
    LOG_D("Removing file: %s \n", filePath.c_str());

    if(!STORAGE.remove(filePath.c_str())){
      LOG_E("Remove FAILED: %s \n", filePath.c_str());
    };
  }
  LOG_D("Removing dir: %s \n", dirName.c_str());
  STORAGE.rmdir(dirName);
}

// Check an delete logs to save space
void checkLogRotate(){
  size_t free = STORAGE.totalBytes() - STORAGE.usedBytes();
  LOG_D("Storage size: %lu K, used: %lu K, free: %lu K\n",STORAGE.totalBytes() / 1024, STORAGE.usedBytes() / 1024,  free/1024);
  if(free < MIN_STORAGE_SPACE){
    String oldestDir = getOldestDir(DATA_DIR);
    LOG_W("Storage is running low, free: %lu K, oldest: %s\n", free/1024, oldestDir.c_str());
    if(oldestDir != "") removeFolder(oldestDir);
  }
}