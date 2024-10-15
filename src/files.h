
#define MIN_STORAGE_SPACE (360 * 1024)  // Minimum allowed space for log rotate to work
#define DATA_DIR "/data"                // Dicectory to save data logs

// Append text at end of a file
void writeFile(const char * path, const char * message) {
  File file = STORAGE.open(path, FILE_APPEND);
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
// List a directory
void listDir(const char * dirname, uint8_t levels) {
  LOG_I("Listing directory: %s\n", dirname);

  File root = STORAGE.open(dirname);
  if (!root) {
    LOG_E("- failed to open directory\n");
    return;
  }
  if (!root.isDirectory()) {
    LOG_E(" - not a directory\n");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      LOG_I("  Dir: %s\n", file.name());
      if (levels) {
        listDir(file.name(), levels - 1);
      }
    } else {
      LOG_I("  File: %s, sz: %lu\n", file.path(), file.size());
    }
    file = root.openNextFile();
  }
}
//List dir and files sorded by name disc
bool listSortedDir(String dirName, std::vector<String> &dirArr, std::vector<std::vector<String>> &fileArr ) {
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
    String dirPath = filePath.substring( 0, filePath.lastIndexOf("/") );
    String fileName = filePath.substring( filePath.lastIndexOf("/") + 1);

    if(oPath != dirPath ){
      oPath = dirPath;
      dirArr.push_back(dirPath);
    }
    String subDir = filePath.substring( 0, filePath.lastIndexOf("/") );
    LOG_D("List Dir: %s, sub Dir: %s  file: %s, sz: %lu\n",
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
    LOG_E("Failed to open dir: %s\n", dirName.c_str());
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
      LOG_D("dir: %s, file: %s\n", dirPath.c_str(), fileName.c_str());
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
    LOG_W("Storage is running low, free: %lu K\n", free/1024);
    String oldestDir = getOldestDir(DATA_DIR);
    removeFolder(oldestDir);
  }
}