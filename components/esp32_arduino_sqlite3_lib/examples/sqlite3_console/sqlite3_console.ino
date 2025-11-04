/*
  Open and execute SQL statements throught this console.
  Output is in tab-delimited format.
    Connections for SD Card in SPI Mode:
      * SD Card   | ESP32
      *  DAT2        -
      *  DAT3        SS (D5)
      *  CMD         MOSI (D23)
      *  VSS         GND
      *  VDD         3.3V
      *  CLK         SCK (D18)
      *  DAT0        MISO (D19)
      *  DAT1        -
    Connections for SD Card in SD_MMC Mode:
      * SD Card   | ESP32
      *  DAT2 (1)    D12
      *  DAT3 (2)    D13
      *  CMD  (3)    D15
      *  VDD  (4)    3.3V
      *  CLK  (5)    D14
      *  VSS  (6)    GND
      *  DAT0 (7)    D2
      *  DAT1 (8)    D4
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <SPI.h>
#include <FS.h>
#include "SPIFFS.h"
#include "SD_MMC.h"
#include "SD.h"

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
#define FORMAT_SPIFFS_IF_FAILED true
#define MAX_FILE_NAME_LEN 100
#define MAX_STR_LEN 500

char db_file_name[MAX_FILE_NAME_LEN] = "\0";
sqlite3 *db = NULL;
int rc;

bool first_time = false;
static int callback(void *data, int argc, char **argv, char **azColName) {
  int i;
  if (first_time) {
     Serial.println((const char *) data);
     for (i = 0; i<argc; i++) {
         if (i)
           Serial.print((char) '\t');
         Serial.printf("%s", azColName[i]);
     }
     Serial.printf("\n");
     first_time = false;
  }
  for (i = 0; i<argc; i++) {
    if (i)
      Serial.print((char) '\t');
    Serial.printf("%s", argv[i] ? argv[i] : "NULL");
  }
  Serial.printf("\n");
  return 0;
}

int db_open() {
  if (db != NULL)
    sqlite3_close(db);
  int rc = sqlite3_open(db_file_name, &db);
  if (rc) {
    Serial.print(F("Can't open database: "));
    Serial.print(sqlite3_extended_errcode(db));
    Serial.print(" ");
    Serial.println(sqlite3_errmsg(db));
    return rc;
  } else
    Serial.println(F("Opened database successfully"));
  return rc;
}

char *zErrMsg = 0;
const char* data = "Output:";
int db_exec(const char *sql) {
  if (db == NULL) {
    Serial.println("No database open");
    return 0;
  }
  first_time = true;
  long start = micros();
  int rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.print(F("SQL error: "));
    Serial.print(sqlite3_extended_errcode(db));
    Serial.print(" ");
    Serial.println(zErrMsg);
    sqlite3_free(zErrMsg);
  } else
    Serial.println(F("Operation done successfully"));
  Serial.print(F("Time taken:"));
  Serial.print(micros()-start);
  Serial.println(F(" us"));
  return rc;
}

int input_string(char *str, int max_len) {
  max_len--;
  int ctr = 0;
  str[ctr] = 0;
  while (str[ctr] != '\n') {
    if (Serial.available()) {
        str[ctr] = Serial.read();
        if (str[ctr] >= ' ' && str[ctr] <= '~')
          ctr++;
        if (ctr >= max_len)
          break;
    }
  }
  str[ctr] = 0;
  Serial.println(str);
  return ctr;
}

int input_num() {
  char in[20];
  int ctr = 0;
  in[ctr] = 0;
  while (in[ctr] != '\n') {
    if (Serial.available()) {
        in[ctr] = Serial.read();
        if (in[ctr] >= '0' && in[ctr] <= '9')
            ctr++;
        if (ctr >= sizeof(in))
          break;
    }
  }
  in[ctr] = 0;
  int ret = atoi(in);
  Serial.println(ret);
  return ret;
}

void listDir(fs::FS &fs, const char * dirname) {
  Serial.print(F("Listing directory: "));
  Serial.println(dirname);
  File root = fs.open(dirname);
  if (!root){
    Serial.println(F("Failed to open directory"));
    return;
  }
  if (!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print(" Dir : ");
      Serial.println(file.name());
    } else {
      Serial.print(" File: ");
      Serial.print(file.name());
      Serial.print(" Size: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println(F("File renamed"));
  } else {
    Serial.println(F("Rename failed"));
  }
}

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println(F("File deleted"));
  } else {
    Serial.println(F("Delete failed"));
  }
}

enum {CHOICE_OPEN_DB = 1, CHOICE_EXEC_SQL, CHOICE_EXEC_MULTI_SQL, CHOICE_CLOSE_DB,
    CHOICE_LIST_FOLDER, CHOICE_RENAME_FILE, CHOICE_DELETE_FILE, CHOICE_SHOW_FREE_MEM};
int askChoice() {
  Serial.println();
  Serial.println(F("Welcome to SQLite console!!"));
  Serial.println(F("---------------------------"));
  Serial.println();
  Serial.print(F("Database file: "));
  Serial.println(db_file_name);
  Serial.println();
  Serial.println(F("1. Open database"));
  Serial.println(F("2. Execute SQL"));
  Serial.println(F("3. Execute Multiple SQL"));
  Serial.println(F("4. Close database"));
  Serial.println(F("5. List folder contents"));
  Serial.println(F("6. Rename file"));
  Serial.println(F("7. Delete file"));
  Serial.println(F("8. Show free memory"));
  Serial.println();
  Serial.print(F("Enter choice: "));
  return input_num();
}

void displayPrompt(const char *title) {
  Serial.println(F("(prefix /spiffs/ or /sd/ or /sdcard/ for"));
  Serial.println(F(" SPIFFS or SD_SPI or SD_MMC respectively)"));
  Serial.print(F("Enter "));
  Serial.println(title);
}

const char *prefixSPIFFS = "/spiffs/";
const char *prefixSD_SPI = "/sd/";
const char *prefixSD_MMC = "/sdcard/";
fs::FS *ascertainFS(const char *str, int *prefix_len) {
  if (strstr(str, prefixSPIFFS) == str) {
    *prefix_len = strlen(prefixSPIFFS) - 1;
    return &SPIFFS;
  }
  if (strstr(str, prefixSD_SPI) == str) {
    *prefix_len = strlen(prefixSD_SPI) - 1;
    return &SD;
  }
  if (strstr(str, prefixSD_MMC) == str) {
    *prefix_len = strlen(prefixSD_MMC) - 1;
    return &SD_MMC;
  }
  return NULL;
}

void setup() {
  Serial.begin(115200);
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.println(F("Failed to mount file Serial"));
    return;
  }
  SPI.begin();
  SD_MMC.begin();
  SD.begin();
  sqlite3_initialize();
}

char str[MAX_STR_LEN];
void loop() {

  int choice = askChoice();
  switch (choice) {
      case CHOICE_OPEN_DB:
        displayPrompt("file name: ");
        input_string(str, MAX_FILE_NAME_LEN);
        if (str[0] != 0) {
          strncpy(db_file_name, str, MAX_FILE_NAME_LEN);
          db_open();
        }
        break;
      case CHOICE_EXEC_SQL:
        Serial.print(F("Enter SQL (max "));
        Serial.print(MAX_STR_LEN);
        Serial.println(F(" characters):"));
        input_string(str, MAX_STR_LEN);
        if (str[0] != 0)
          db_exec(str);
        break;
      case CHOICE_EXEC_MULTI_SQL:
        Serial.println(F("(Copy paste may not always work due to limited serial buffer)"));
        Serial.println(F("Keep entering SQL, empty to stop:"));
        do {
          input_string(str, MAX_STR_LEN);
          if (str[0] != 0)
            db_exec(str);
        } while (str[0] != 0);
        break;
      case CHOICE_CLOSE_DB:
        if (db_file_name[0] != 0) {
            db_file_name[0] = 0;
            sqlite3_close(db);
        }
        break;
      case CHOICE_LIST_FOLDER:
      case CHOICE_RENAME_FILE:
      case CHOICE_DELETE_FILE:
        fs::FS *fs;
        displayPrompt("path: ");
        input_string(str, MAX_STR_LEN);
        if (str[0] != 0) {
          int fs_prefix_len = 0;
          fs = ascertainFS(str, &fs_prefix_len);
          if (fs != NULL) {
            switch (choice) {
              case CHOICE_LIST_FOLDER:
                listDir(*fs, str + fs_prefix_len);
                break;
              case CHOICE_RENAME_FILE:
                char str1[MAX_FILE_NAME_LEN];
                displayPrompt("path to rename as: ");
                input_string(str1, MAX_STR_LEN);
                if (str1[0] != 0)
                  renameFile(*fs, str + fs_prefix_len, str1 + fs_prefix_len);
                break;
              case CHOICE_DELETE_FILE:
                deleteFile(*fs, str + fs_prefix_len);
                break;
            }
          }
        }
        break;
      case CHOICE_SHOW_FREE_MEM:
        Serial.printf("\nHeap size: %d\n", ESP.getHeapSize());
        Serial.printf("Free Heap: %d\n", esp_get_free_heap_size());
        Serial.printf("Min Free Heap: %d\n", esp_get_minimum_free_heap_size());
        Serial.printf("Largest Free block: %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        break;
      default:
        Serial.println(F("Invalid choice. Try again."));
  }

}