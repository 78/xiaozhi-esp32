/*
    This utility provides ability to run Sql Statements on  Sqlite3 databases from SD Card
    or SPIFFS through the Web Server and display in the form of HTML page.

    For more information, visit https://github.com/siara-cc/esp32_arduino_sqlite3_lib

    Copyright (c) 2018, Siara Logics (cc)
*/

/*
   Copyright (c) 2015, Majenko Technologies
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

 * * Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

 * * Redistributions in binary form must reproduce the above copyright notice, this
     list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.

 * * Neither the name of Majenko Technologies nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <sqlite3.h>
#include <SPI.h>
#include <FS.h>
#include "SPIFFS.h"
#include "SD_MMC.h"
#include "SD.h"

const char *ssid = "Nokia1";
const char *password = "nokiafive";

WebServer server(80);

const int led = 13;

void handleRoot(const char *db_name, const char *sql) {
  digitalWrite ( led, 1 );
  String temp;
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  temp = "<html><head>\
      <title>ESP32 Sqlite Web Console</title>\
      <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; font-size: large; Color: #000088; }\
      </style>\
  </head>\
  <body>\
      <h2>ESP32 Sqlite Web Console</h2>\
      <p>Uptime: ";
  temp += hr;
  temp += ":";
  temp += min % 60;
  temp += ":";
  temp += sec % 60;
  temp += "</p>\
      <form name='params' method='POST' action='exec_sql'>\
      <textarea style='font-size: medium; width:100%' rows='4' placeholder='Enter SQL Statement' name='sql'>";
  if (sql != NULL)
    temp += sql;
  temp += "</textarea> \
      <br>File name (prefix with /spiffs/ or /sd/ or /sdcard/):<br/><input type=text size='50' style='font-size: small' value='";
  if (db_name != NULL)
    temp += db_name;
  temp += "' name='db_name'/> \
      <br><br><input type=submit style='font-size: large' value='Execute'/>\
      </form><hr/>";

  server.send ( 200, "text/html", temp.c_str() );
  digitalWrite ( led, 0 );
}

void handleNotFound() {
  digitalWrite ( led, 1 );
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
      message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
  digitalWrite ( led, 0 );
}

sqlite3 *db1;
int rc;
sqlite3_stmt *res;
int rec_count = 0;
const char *tail;
char current_db[255];

int openDb(const char *filename) {
  if (strncmp(filename, current_db, sizeof(current_db)) == 0)
    return 0;
  else
    sqlite3_close(db1);
  int rc = sqlite3_open(filename, &db1);
  if (rc) {
      Serial.printf("Can't open database: %s\n", sqlite3_errmsg(db1));
      memset(current_db, '\0', sizeof(current_db));
      return rc;
  } else {
      Serial.printf("Opened database successfully\n");
      strcpy(current_db, filename);
  }
  return rc;
}

void setup ( void ) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while ( WiFi.status() != WL_CONNECTED ) {
      delay ( 500 );
      Serial.print ( "." );
  }

  Serial.println ( "" );
  Serial.print ( "Connected to " );
  Serial.println ( ssid );
  Serial.print ( "IP address: " );
  Serial.println ( WiFi.localIP() );

  if ( MDNS.begin ( "esp32" ) ) {
      Serial.println ( "MDNS responder started" );
  }

  memset(current_db, '\0', sizeof(current_db));

  if (!SPIFFS.begin(true)) {
    Serial.println(F("Failed to mount file Serial"));
    return;
  }
  SPI.begin();
  SD_MMC.begin();
  SD.begin();
  sqlite3_initialize();

  server.on ( "/", []() {
    handleRoot(NULL, NULL);
  });
  server.on ( "/exec_sql", []() {
      String db_name = server.arg("db_name");
      String sql = server.arg("sql");
      server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      handleRoot(db_name.c_str(), sql.c_str());
      if (openDb(db_name.c_str())) {
        String resp = "Error opening database: ";
        resp += sqlite3_errmsg(db1);
        resp += "<pre>";
        resp += server.arg("db_name");
        resp += "</pre>";
        resp += ".<br><br><input type=button onclick='location.href=\"/\"' value='back'/>";
        server.sendContent(resp);
        return;
      }
      //String sql = "Select count(*) from gendered_names where name between '";
      //sql += server.arg("from");
      //sql += "' and '";
      //sql += server.arg("to");
      //sql += "'";
      //rc = sqlite3_prepare_v2(db1, sql.c_str(), -1, &res, &tail);
      //if (rc != SQLITE_OK) {
      //    String resp = "Failed to fetch data: ";
      //    resp += sqlite3_errmsg(db1);
      //    resp += ".<br><br><input type=button onclick='location.href=\"/\"' value='back'/>";
      //    server.send ( 200, "text/html", resp.c_str());
      //    Serial.println(resp.c_str());
      //    return;
      //}
      //while (sqlite3_step(res) == SQLITE_ROW) {
      //    rec_count = sqlite3_column_int(res, 0);
      //    if (rec_count > 5000) {
      //        String resp = "Too many records: ";
      //        resp += rec_count;
      //        resp += ". Please select different range";
      //        resp += ".<br><br><input type=button onclick='location.href=\"/\"' value='back'/>";
      //        server.send ( 200, "text/html", resp.c_str());
      //        Serial.println(resp.c_str());
      //        sqlite3_finalize(res);
      //        return;
      //    }
      //}
      //sqlite3_finalize(res);

      rc = sqlite3_prepare_v2(db1, sql.c_str(), -1, &res, &tail);
      if (rc != SQLITE_OK) {
          String resp = "Failed to fetch data: ";
          resp += sqlite3_errmsg(db1);
          resp += "<br><br><a href='/'>back</a>";
          server.sendContent(resp);
          Serial.println(resp.c_str());
          return;
      }

      rec_count = 0;
      String resp = "<h2>Result:</h2><h3>";
      resp += sql;
      resp += "</h3><table cellspacing='1' cellpadding='1' border='1'>";
      server.sendContent(resp);
      bool first = true;
      while (sqlite3_step(res) == SQLITE_ROW) {
          resp = "";
          if (first) {
            int count = sqlite3_column_count(res);
            if (count == 0) {
                resp += "<tr><td>Statement executed successfully</td></tr>";
                rec_count = sqlite3_changes(db1);
                break;
            }
            resp += "<tr>";
            for (int i = 0; i<count; i++) {
                resp += "<td>";
                resp += sqlite3_column_name(res, i);
                resp += "</td>";
            }
            resp += "</tr>";
            first = false;
          }
          int count = sqlite3_column_count(res);
          resp += "<tr>";
          for (int i = 0; i<count; i++) {
              resp += "<td>";
              resp += (const char *) sqlite3_column_text(res, i);
              resp += "</td>";
          }
          resp += "</tr>";
          server.sendContent(resp);
          rec_count++;
      }
      resp = "</table><br><br>Number of records: ";
      resp += rec_count;
      resp += ".<br><br><input type=button onclick='location.href=\"/\"' value='back'/>";
      server.sendContent(resp);
      sqlite3_finalize(res);
  } );
  server.onNotFound ( handleNotFound );
  server.begin();
  Serial.println ( "HTTP server started" );
}

void loop ( void ) {
  server.handleClient();
}
