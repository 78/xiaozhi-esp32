/* Example to create a table and insert 5000 bytes into blob column.
   Creates the database on SD Card connected to the SDMMC port.
*/
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <SPI.h>
#include <FS.h>
#include "SD_MMC.h"

const char* data = "Callback function called";
static int callback(void *data, int argc, char **argv, char **azColName){
   int i;
   Serial.printf("%s: ", (const char*)data);
   for (i = 0; i<argc; i++){
       Serial.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   Serial.printf("\n");
   return 0;
}

int openDb(const char *filename, sqlite3 **db) {
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

#define DATA_SIZE 5000

void setup() {
   Serial.begin(115200);
   sqlite3 *db1;
   sqlite3_stmt *res;
   const char *tail;
   int rc;

   char *data = (char *) malloc(DATA_SIZE);

   SPI.begin();
   SD_MMC.begin();

   sqlite3_initialize();

   // Open database 1
   if (openDb("/sdcard/test_long_data.db", &db1))
       return;

   rc = db_exec(db1, "CREATE TABLE IF NOT EXISTS test (c1 blob)");
   if (rc != SQLITE_OK) {
       sqlite3_close(db1);
       return;
   }

   for (int i = 0; i < DATA_SIZE; i++) {
     data[i] = 'a' + i % 26;
   }
   const char *sql = "INSERT INTO test VALUES (?)";
   rc = sqlite3_prepare_v2(db1, sql, strlen(sql), &res, &tail);
   if (rc != SQLITE_OK) {
     Serial.printf("ERROR preparing sql: %s\n", sqlite3_errmsg(db1));
     sqlite3_close(db1);
     return;
   }
   sqlite3_bind_blob(res, 1, data, DATA_SIZE, SQLITE_STATIC);
   if (sqlite3_step(res) != SQLITE_DONE) {
     Serial.printf("ERROR inserting data: %s\n", sqlite3_errmsg(db1));
     sqlite3_close(db1);
     return;
   }
   sqlite3_finalize(res);

   rc = db_exec(db1, "Select length(c1), substr(c1, 1, 10) from test");
   if (rc != SQLITE_OK) {
     sqlite3_close(db1);
     return;
   }

   sqlite3_close(db1);

}

void loop() {
}

