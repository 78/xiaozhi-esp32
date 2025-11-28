/*
    This example shows how to retrieve data from Sqlite3 databases from SD Card
    through the Web Server and display in the form of HTML page.
    It also demonstrates query filtering by parameter passing and chunked encoding.
    Before running please copy following files to SD Card:

    data/babyname.db

    This database contains around 30000 baby names and corresponding data.

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

  temp = "<html><head>\
      <title>ESP32 Demo</title>\
      <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; font-size: large; Color: #000088; }\
      </style>\
  </head>\
  <body>\
      <h1>Hello from ESP32!</h1>\
      <p>Uptime: ";
  temp += hr;
  temp += ":";
  temp += min % 60;
  temp += ":";
  temp += sec % 60;
  temp += "</p>\
      <h2>Query gendered names database</h2>\
      <form name='params' method='GET' action='query_db'>\
      Enter from: <input type=text style='font-size: large' value='Bob' name='from'/> \
      <br>to: <input type=text style='font-size: large' value='Bobby' name='to'/> \
      <br><br><input type=submit style='font-size: large' value='Query database'/>\
      </form>\
  </body>\
  </html>";

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

  SD_MMC.begin();
  sqlite3_initialize();

  // Open database
  if (openDb("/sdcard/babyname.db", &db1))
      return;

  server.on ( "/", handleRoot );
  server.on ( "/query_db", []() {
      String sql = "Select count(*) from gendered_names where name between '";
      sql += server.arg("from");
      sql += "' and '";
      sql += server.arg("to");
      sql += "'";
      rc = sqlite3_prepare_v2(db1, sql.c_str(), -1, &res, &tail);
      if (rc != SQLITE_OK) {
          String resp = "Failed to fetch data: ";
          resp += sqlite3_errmsg(db1);
          resp += ".<br><br><input type=button onclick='location.href=\"/\"' value='back'/>";
          server.send ( 200, "text/html", resp.c_str());
          Serial.println(resp.c_str());
          return;
      }
      while (sqlite3_step(res) == SQLITE_ROW) {
          rec_count = sqlite3_column_int(res, 0);
          if (rec_count > 5000) {
              String resp = "Too many records: ";
              resp += rec_count;
              resp += ". Please select different range";
              resp += ".<br><br><input type=button onclick='location.href=\"/\"' value='back'/>";
              server.send ( 200, "text/html", resp.c_str());
              Serial.println(resp.c_str());
              sqlite3_finalize(res);
              return;
          }
      }
      sqlite3_finalize(res);

      sql = "Select year, state, name, total_babies, primary_sex, primary_sex_ratio, per_100k_in_state from gendered_names where name between '";
      sql += server.arg("from");
      sql += "' and '";
      sql += server.arg("to");
      sql += "'";
      rc = sqlite3_prepare_v2(db1, sql.c_str(), -1, &res, &tail);
      if (rc != SQLITE_OK) {
          String resp = "Failed to fetch data: ";
          resp += sqlite3_errmsg(db1);
          resp += "<br><br><a href='/'>back</a>";
          server.send ( 200, "text/html", resp.c_str());
          Serial.println(resp.c_str());
          return;
      }

      rec_count = 0;
      server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      String resp = "<html><head><title>ESP32 Sqlite local database query through web server</title>\
          <style>\
          body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; font-size: large; Color: #000088; }\
          </style><head><body><h1>ESP32 Sqlite local database query through web server</h1><h2>";
      resp += sql;
      resp += "</h2><br><table cellspacing='1' cellpadding='1' border='1'><tr><td>Year</td><td>State</td><td>Name</td><td>Total babies</td><td>Primary Sex</td><td>Ratio</td><td>Per 100k</td></tr>";
      server.send ( 200, "text/html", resp.c_str());
      while (sqlite3_step(res) == SQLITE_ROW) {
          resp = "<tr><td>";
          resp += sqlite3_column_int(res, 0);
          resp += "</td><td>";
          resp += (const char *) sqlite3_column_text(res, 1);
          resp += "</td><td>";
          resp += (const char *) sqlite3_column_text(res, 2);
          resp += "</td><td>";
          resp += sqlite3_column_int(res, 3);
          resp += "</td><td>";
          resp += (const char *) sqlite3_column_text(res, 4);
          resp += "</td><td>";
          resp += sqlite3_column_double(res, 5);
          resp += "</td><td>";
          resp += sqlite3_column_double(res, 6);
          resp += "</td></tr>";
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
