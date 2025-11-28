/*
    This creates two empty databases, populates values, and retrieves them back
    from the SPIFFS file 
*/
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <SPI.h>
#include <FS.h>
#include "SPIFFS.h"

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
#define FORMAT_SPIFFS_IF_FAILED true

const char* data = "Callback function called";
static int callback(void *data, int argc, char **argv, char **azColName) {
   int i;
   Serial.printf("%s: ", (const char*)data);
   for (i = 0; i<argc; i++){
       Serial.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   Serial.printf("\n");
   return 0;
}

int db_open(const char *filename, sqlite3 **db) {
   int rc = sqlite3_open(filename, db);
   if (rc) {
       Serial.printf("Can't open database: %s\n", sqlite3_errmsg(*db));
       return rc;
   } else {
       Serial.printf("Opened database successfully\n");
   }
   return rc;
}

char *zErrMsg = 0;
int db_exec(sqlite3 *db, const char *sql) {
   Serial.println(sql);
   long start = micros();
   int rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
   if (rc != SQLITE_OK) {
       Serial.printf("SQL error: %s\n", zErrMsg);
       sqlite3_free(zErrMsg);
   } else {
       Serial.printf("Operation done successfully\n");
   }
   Serial.print(F("Time taken:"));
   Serial.println(micros()-start);
   return rc;
}

void setup() {

   Serial.begin(115200);
   sqlite3 *db1;
   sqlite3 *db2;
   int rc;

   if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
       Serial.println("Failed to mount file system");
       return;
   }

   // list SPIFFS contents
   File root = SPIFFS.open("/");
   if (!root) {
       Serial.println("- failed to open directory");
       return;
   }
   if (!root.isDirectory()) {
       Serial.println(" - not a directory");
       return;
   }
   File file = root.openNextFile();
   while (file) {
       if (file.isDirectory()) {
           Serial.print("  DIR : ");
           Serial.println(file.name());
       } else {
           Serial.print("  FILE: ");
           Serial.print(file.name());
           Serial.print("\tSIZE: ");
           Serial.println(file.size());
       }
       file = root.openNextFile();
   }

   // remove existing file
   SPIFFS.remove("/test1.db");
   SPIFFS.remove("/test2.db");

   sqlite3_initialize();

   if (db_open("/spiffs/test1.db", &db1))
       return;
   if (db_open("/spiffs/test2.db", &db2))
       return;

   rc = db_exec(db1, "CREATE TABLE test1 (id INTEGER, content);");
   if (rc != SQLITE_OK) {
       sqlite3_close(db1);
       sqlite3_close(db2);
       return;
   }
   rc = db_exec(db2, "CREATE TABLE test2 (id INTEGER, content);");
   if (rc != SQLITE_OK) {
       sqlite3_close(db1);
       sqlite3_close(db2);
       return;
   }

   rc = db_exec(db1, "INSERT INTO test1 VALUES (1, 'Hello, World from test1');");
   if (rc != SQLITE_OK) {
       sqlite3_close(db1);
       sqlite3_close(db2);
       return;
   }
   rc = db_exec(db2, "INSERT INTO test2 VALUES (1, 'Hello, World from test2');");
   if (rc != SQLITE_OK) {
       sqlite3_close(db1);
       sqlite3_close(db2);
       return;
   }

   rc = db_exec(db1, "SELECT * FROM test1");
   if (rc != SQLITE_OK) {
       sqlite3_close(db1);
       sqlite3_close(db2);
       return;
   }
   rc = db_exec(db2, "SELECT * FROM test2");
   if (rc != SQLITE_OK) {
       sqlite3_close(db1);
       sqlite3_close(db2);
       return;
   }

   sqlite3_close(db1);
   sqlite3_close(db2);

}

void loop() {
}
