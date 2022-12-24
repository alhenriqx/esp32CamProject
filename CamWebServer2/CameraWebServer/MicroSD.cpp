#include "FS.h"
#include "SD_MMC.h"

void removeAllFiles(fs::FS &fs, const char *dirname)
{
   File root = fs.open(dirname, FILE_WRITE);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    char filenameBuffer[80];
    File file = root.openNextFile(FILE_WRITE);
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
        } else {            
            
            Serial.printf("Deleting FILE:%s SIZE:%d\n\r", file.name(), file.size());
            
            sprintf(filenameBuffer, "/pics/%s", file.name());
            Serial.printf("removing %s\n\r", filenameBuffer);
            if (!fs.remove(filenameBuffer))
            {
              Serial.printf("failed to remove %s\n\r", filenameBuffer);
            }
        }
        file = root.openNextFile(FILE_WRITE);
    }
}


void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void readFile2(File file, uint8_t **ppbuf, int *pLen) {

    size_t size = file.size();

    uint8_t *buf = (uint8_t*)malloc(sizeof(uint8_t) * size);

    if (buf != NULL)
    {
      file.read(buf, size);
    }
    else
    {
      Serial.println("mem alloc to read failed");
      size = 0;
    }
    //file.close();

    *ppbuf = buf;
    *pLen = (int)size;
}

void readFile(fs::FS &fs, const char * path, uint8_t **ppbuf, int *pLen){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    size_t size = file.size();

    uint8_t *buf = (uint8_t*)malloc(sizeof(uint8_t) * size);

    if (buf != NULL)
    {
      file.read(buf, size);
    }
    else
    {
      Serial.println("mem alloc to read failed");
      size = 0;
    }
    file.close();

    *ppbuf = buf;
    *pLen = (int)size;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}
