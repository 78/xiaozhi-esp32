/*
    This example shows how to retrieve data from Sqlite3 databases from SD Card
    through the Web Server and display in the form of HTML page.
    It also demonstrates query filtering by parameter passing and chunked encoding.
    Before running please copy 'so_users.db' to SD Card. Please see

    https://github.com/siara-cc/stackoverflow_db

    to find out how to obtain so_users.db

    Please see https://github.com/siara-cc/esp32_arduino_sqlite3_lib/
    for more inforemation.

    Copyright (c) 2018, Siara Logics (cc)
*/

/*
* Copyright (c) 2015, Majenko Technologies
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice, this
*   list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above copyright notice, this
*   list of conditions and the following disclaimer in the documentation and/or
*   other materials provided with the distribution.
*
* * Neither the name of Majenko Technologies nor the names of its
*   contributors may be used to endorse or promote products derived from
*   this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <sqlite3.h>
#include <FS.h>
#include "SD_MMC.h"

const char *ssid = "Nokia1";
const char *password = "nokiafour";

WebServer server(80);

const int led = 13;

void handleRoot() {
  digitalWrite ( led, 1 );
  String temp;
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  temp = F("<html><head>\
      <title>ESP32 Demo to Query database on Micro SD</title>\
      <style>\
        body { font-family: Arial, Helvetica, Sans-Serif; font-size: large; Color: #000088; }\
      </style>\
  </head>\
  <body>\
      <h1>Hello from ESP32!</h1>\
      <h2>Query StackOverflow Users database on Micro SD Card</h2>\
      <p>StackOverflow publishes snapshot of its data periodically at archive.org \
      <a href='https://archive.org/download/stackexchange'>here</a> \
      and is <a href='https://ia800107.us.archive.org/27/items/stackexchange/license.txt'> \
      licensed under cc-by-sa 3.0</a>.</p>\
      <p><a href='https://github.com/siara-cc/stackoverflow_db'>This repository</a> \
      hosts StackOverflow User data imported into a convenient SQLite database\
      (size 1.94 GB and contains close to 10 million records). The date of snapshot is 3-Dec-2018.</p>\
      <p>This example shows how to retrieve data from this database copied to Micro SD Card \
      attached to ESP32 through its Web Server and display in the form of HTML page.</p>\
      <h3>Query by User Id</h3>\
        <form name='params' method='GET' action='query_db'>\
          Enter id: <input type=text style='font-size: large' value='5072621' name='so_id'/> \
          <input type=hidden value='' name='so_disp_name'/> \
          <p>(To find your id, see search box after clicking your profile icon on https://stackoverflow.com)</p>\
          <input type=submit style='font-size: large' value='Query database by Id'/>\
        </form>\
        <hr>\
      <h3>Query by Display name</h3>\
        <form name='params' method='GET' action='query_db'>\
          <input type=hidden value='' name='so_id'/> \
          Enter Display name: <input type=text style='font-size: large' value='roadrunner' name='so_disp_name'/> \
          <br><br><input type=submit style='font-size: large' value='Query database by Display Name'/>\
        </form>\
      <hr>\
      <h3>Aggregate Query by Location</h3>\
        <form name='params' method='GET' action='query_db'>\
          <input type=hidden value='' name='so_id'/> \
          <input type=hidden value='' name='so_disp_name'/> \
          Enter Location (blank for all): \
          <input type=text style='font-size: large' value='' name='so_loc'/> \
          <br>Minimum count:\
          <input type=text style='font-size: large' value='10000' name='so_loc_count'/> \
          <br><br><input type=submit style='font-size: large' value='Aggregate Query by Location'/>\
        </form>\
      <hr>\
  </body>\
  </html>");
  server.send(200, "text/html", temp.c_str());
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = F("File Not Found\n\n");
  message += F("URI: ");
  message += server.uri();
  message += F("\nMethod: ");
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += F("\nArguments: ");
  message += server.args();
  message += F("\n");
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

sqlite3 *db1;
int rc;
sqlite3_stmt *res;
int rec_count = 0;
const char *tail;

int openDb(const char *filename, sqlite3 **db) {
  int rc = sqlite3_open(filename, db);
  if (rc) {
    Serial.print(F("Can't open database: "));
    Serial.println(sqlite3_errmsg(*db));
    return rc;
  } else {
    Serial.println(F("Opened database successfully"));
  }
  return rc;
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println(F("Hello"));

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }

  Serial.println("");
  Serial.print(F("Connected to "));
  Serial.println(ssid);
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32")) {
    Serial.println(F("MDNS responder started"));
  }

  SD_MMC.begin();
  sqlite3_initialize();

  // Open database
  if (openDb("/sdcard/so_users.db", &db1))
    return;

  server.on("/", handleRoot);
  server.on("/query_db", []() {
    long start = micros();
    String sql = F("Select Count(*) From SO_Users Where ");
    if (server.arg("so_disp_name").length() > 0) {
      sql += F("DisplayName = '");
      sql += server.arg("so_disp_name");
      sql += "'";
    } else if (server.arg("so_id").length() > 0) {
      sql += F("Id = '");
      sql += server.arg("so_id");
      sql += F("'");
    } else {
      sql = "";
    }
    int step_res;
    if (sql.length() > 0) {
      rc = sqlite3_prepare_v2(db1, sql.c_str(), -1, &res, &tail);
      if (rc != SQLITE_OK) {
        String resp = F("Failed to fetch data: ");
        resp += sqlite3_errmsg(db1);
        resp += F(".<br><br><input type=button onclick='location.href=\"/\"' value='back'/>");
        server.send ( 200, "text/html", resp.c_str());
        Serial.println(resp.c_str());
        return;
      }
      do {
        step_res = sqlite3_step(res);
        if (step_res == SQLITE_ROW) {
          rec_count = sqlite3_column_int(res, 0);
          if (rec_count > 5000) {
            String resp = F("Too many records: ");
            resp += rec_count;
            resp += F(". Please select different range");
            resp += F(".<br><br><input type=button onclick='location.href=\"/\"' value='back'/>");
            server.send ( 200, "text/html", resp.c_str());
            Serial.println(resp.c_str());
            sqlite3_finalize(res);
            return;
          }
        }
      } while (step_res != SQLITE_DONE && step_res != SQLITE_ERROR);
      sqlite3_finalize(res);
    } else
      rec_count = -1;

    sql = F("Select * From SO_Users Where ");
    if (server.arg("so_disp_name").length() > 0) {
      sql += F("DisplayName = '");
      sql += server.arg("so_disp_name");
      sql += F("'");
    } else if (server.arg("so_id").length() > 0) {
      sql += F("Id = '");
      sql += server.arg("so_id");
      sql += F("'");
    } else {
      sql = F("Select Location, Count(*) Count From SO_Users ");
      if (server.arg("so_loc").length() > 0) {
        sql += F("Where Location = '");
        sql += server.arg("so_loc");
        sql += F("' ");
      } else
        sql += F("Where Location > '' ");
      sql += F("Group by Location ");
      if (server.arg("so_loc_count").length() > 0) {
        sql += F("Having Count(*) >= ");
        sql += server.arg("so_loc_count");
      }
    }
    rc = sqlite3_prepare_v2(db1, sql.c_str(), -1, &res, &tail);
    if (rc != SQLITE_OK) {
      String resp = F("Failed to fetch data: ");
      resp += sqlite3_errmsg(db1);
      resp += F("<br><br><a href='/'>back</a>");
      server.send ( 200, "text/html", resp.c_str());
      Serial.println(resp.c_str());
      return;
    }

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    String resp = F("<!DOCTYPE html><html><head>\
        <title>StackOverflow Database query on ESP32 through web server</title>\
        <style>\
        body { font-family: Arial, Helvetica, Sans-Serif; font-size: large; Color: #000088; }\
        </style><head><body><h1>Query StackOverflow Users db on Micro SD card attached to ESP32 through its web server</h1><h3>");
    resp += sql;
    resp += F("</h3>");
    if (rec_count >= 0) {
      resp += F("<p>No. of records: ");
      resp += rec_count;
      resp += F("</p>");
    }
    resp += F("<table cellspacing='1' cellpadding='1' border='1'>");
    server.send(200, "text/html", resp.c_str());
    int cols = sqlite3_column_count(res);
    resp = F("<tr>");
    for (int i = 0; i < cols; i++) {
      resp += F("<td>");
      resp += (const char *) sqlite3_column_name(res, i);
      resp += F("</td>");
    }
    resp += F("</tr>");
    server.sendContent(resp);
    do {
      step_res = sqlite3_step(res);
      if (step_res == SQLITE_ROW) {
        resp = F("<tr>");
        for (int i = 0; i < cols; i++) {
          resp += F("<td>");
          resp += (const char *) sqlite3_column_text(res, i);
          resp += F("</td>");
        }
        resp += F("</tr>");
        server.sendContent(resp);
        rec_count++;
      }
    } while (step_res != SQLITE_DONE && step_res != SQLITE_ERROR);
    resp = F("</table>");
    resp += F("<br>Time taken (seconds): ");
    resp += (micros()-start)/1000000;
    resp += F("<br><br><input type=button onclick='location.href=\"/\"' value='back'/>");
    server.sendContent(resp);
    sqlite3_finalize(res);
  });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println(F("HTTP server started"));
}

void loop ( void ) {
  server.handleClient();
}
