#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

const char *dbase = "http://hydroponic.omahiot.com/api/nutrient";  //database
const char* ssid = "Hydroponics_2";
const char* pass = "WWW.omahiot.NET";

String sensords, sensorjsn, sensorph, sensorec, gh_id = "3", delay_kirim = "0";

WiFiClient client;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  connectToWifi();
}

void loop() {
 String msg = "";
  while (Serial.available() > 0) {
    sensorph = Serial.readStringUntil('!');
    sensorec = Serial.readStringUntil('@');
    sensords = Serial.readStringUntil('#');
    sensorjsn = Serial.readStringUntil('$');
    
    Serial.println("pemisah");
    if (checkContain(sensorph) && checkContain(sensorec) && checkContain(sensords) && checkContain(sensorjsn)) {
     
      int tds_int = sensorec.toInt();

      Serial.println(sensorph);
      Serial.println(sensorec);
      Serial.println(sensords);
      Serial.println(sensorjsn);

      if(tds_int < 100){
        Serial.println("Data tidak valid");
      }
      else{
        kirimDataKeServer();  
      }      
      delay(1000);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }

}

void kirimDataKeServer() {
  //--------------start to transmit data sensor to server----------//
  if ((WiFi.status() == WL_CONNECTED)) {
    // Buat objek JSON
    StaticJsonDocument<200> doc;

    // Isi data ke objek JSON
    doc["gh_id"] = gh_id;
    doc["ph"] = sensorph.toFloat();
    doc["tds"] = sensorec.toFloat();
    doc["level_nutrient"] = sensorjsn.toInt();
    doc["delay"] = delay_kirim;
    doc["temperature"] = sensords.toFloat();

    // Buat string JSON dari objek JSON
    String jsonStr;
    serializeJson(doc, jsonStr);

    // Kirim data ke server
    HTTPClient http;
    http.begin(client, dbase);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(jsonStr);

    if (httpResponseCode == 200) {  
      Serial.println("Data is sent successfully");
    } else {
      Serial.print("Error in sending data. Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}


void connectToWifi() {
  WiFi.begin(ssid, pass);
  Serial.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {  //Loop which makes a point every 500ms until the connection process has finished
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP-Address: ");
  Serial.println(WiFi.localIP());
}

String splitString(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

bool checkContain(String text) {
  bool status = true;
  if (text.indexOf("x") > 0 || text.indexOf("!") > 0 || text.indexOf("@") > 0
      || text.indexOf("#") > 0 || text.indexOf("$") > 0 || text.indexOf("%") > 0
      || text.indexOf("&") > 0 || text.indexOf("x") > 0) {
    status = false;
  }
  return status;
}
