#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "esp_task_wdt.h"
#include "time.h"
#include "Wire.h"

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 0;
char timeStringBuff[30];
String timeString;

#define LED 2

const char* ssid = "Hydroponics_2";
const char* password = "WWW.omahiot.NET";
const char* serverUrlWeather = "https://hydroponic.omahiot.com/api/getNutrientWeather/3";
const char* serverUrlControlling = "https://hydroponic.omahiot.com/api/getControlling?gh_id=3";

float suhu_max = 38.00, ph_max, tds_max, jsn_max = 38.00, temperature, humidity, light, ph, tds, level_nutrient, suhu_air;
String gh_id, node, server_time, accumulatedSensorData, time_str, waktu_ntp;

unsigned long lastTime = 0;
unsigned long timerDelay = 800000;
// unsigned long timerDelay = 60000;

String pompaph, pompanuta, pompanutb, blow;

// Controlling
const int relayPh = 14;
const int relayNutA = 27;
const int relayNutB = 26;
const int relayblower = 25;

bool pumpA = false;
bool pumpB = false;
bool pumpPh = false;
unsigned long prevMilPh = 0;
unsigned long prevMilA = 0;
unsigned long prevMilB = 0;


void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(10000);

  lcd.init();
  lcd.backlight();

   // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  pinMode(LED, OUTPUT);
  pinMode(relayPh, OUTPUT);
  pinMode(relayNutA, OUTPUT);
  pinMode(relayNutB, OUTPUT);
  pinMode(relayblower, OUTPUT);

  digitalWrite(relayNutA, LOW);
  digitalWrite(relayNutB, LOW);
  digitalWrite(relayPh, LOW);
  digitalWrite(relayblower, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  digitalWrite(LED, HIGH);
  Serial.println("WiFi connected");

  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }

  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  // Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");

  // esp_task_wdt_init(8, true);
  // esp_task_wdt_add(NULL);

  // Menunggu sinkronisasi waktu
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "Current time: %Y-%m-%d %H:%M:%S");
  createFileIfNotExists(); 
}

void loop() {
  //esp_task_wdt_reset();

  if (millis() - lastTime > timerDelay) {
    if (WiFi.status() == WL_CONNECTED) {
      //esp_task_wdt_reset();
      getDataFromWeatherAPI();
      getDataFromControllingAPI();
      // processSensorData();
      lcd.clear();
      blower();
      controllingPH();
      controllingTDSA();
      controllingTDSB();
    } else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }

  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
}

void createFileIfNotExists() {
  File file = SD.open("/relay_log.txt", FILE_WRITE);
  if (file) {
      file.close();
      Serial.println("File created successfully");
  } else {
      Serial.println("Failed to create file");
  }
  file.close();
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());
}

void getDataFromWeatherAPI() {
  HTTPClient http;
  http.begin(serverUrlWeather);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Weather Response: " + payload);

    DynamicJsonDocument doc(2048); // Increase capacity
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      server_time = doc["weather"]["server_time"].as<String>();
      node = doc["weather"]["node"].as<String>();
      temperature = doc["weather"]["temperature"].as<float>();
      humidity = doc["weather"]["humidity"].as<float>();
      light = doc["weather"]["light"].as<float>();
      gh_id = doc["weather"]["gh_id"].as<String>();
      ph = doc["nutrient"]["ph"].as<float>();
      tds = doc["nutrient"]["tds"].as<float>();
      level_nutrient = doc["nutrient"]["level_nutrient"].as<float>();
      suhu_air = doc["nutrient"]["temperature"].as<float>();
      
      int spaceIndex = server_time.indexOf(' ');
      time_str = server_time.substring(spaceIndex + 1);
      
      Serial.println("Weather Data:");
      Serial.print("Server Time: "); Serial.println(server_time);
      Serial.print("Server Time: "); Serial.println(time_str);
      Serial.print("GH ID: "); Serial.println(gh_id);
      Serial.print("Node: "); Serial.println(node);
      Serial.print("Temperature: "); Serial.println(temperature);
      Serial.print("Humidity: "); Serial.println(humidity);
      Serial.print("Light: "); Serial.println(light);
      Serial.print("PPm: "); Serial.println(tds);
      Serial.print("Nutrient Level: "); Serial.println(level_nutrient);
      Serial.println();
    } else {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("Error on Weather HTTP request: ");
    Serial.println(httpCode);
  }
  http.end();
}

void getDataFromControllingAPI() {
  HTTPClient http;
  http.begin(serverUrlControlling);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Controlling Response: " + payload);

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      suhu_max = doc["suhu_max"].as<float>();
      ph_max = doc["ph_max"].as<float>();
      tds_max = doc["tds_max"].as<float>();

      // Serial.println("Controlling Data:");
      // Serial.print("Suhu max: "); Serial.println(suhu_max);
      // Serial.print("Ph max: "); Serial.println(ph_max);
      // Serial.print("TDS max: "); Serial.println(tds_max);
    } else {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("Error on Controlling HTTP request: ");
    Serial.println(httpCode);
  }
  http.end();
}

void blower(){
  File logFile = SD.open("/relay_log.txt", FILE_APPEND);
  if (!logFile) {
    Serial.println("Failed to open file for writing");
    return;
  }

  // Tambahkan fungsi jam terlebih dahulu
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  strftime(timeStringBuff, sizeof(timeStringBuff), "%d %B %Y %H:%M:%S", &timeinfo);
  timeString = String(timeStringBuff);

  // masuk program blower utama
  if (temperature > suhu_max){
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.print("Blower menyala dengan suhu sekarang di ");  Serial.print(temperature);  Serial.println("*C");
    logFile.print(timeString);   logFile.print("  -->  ");  logFile.print("Blower menyala dengan suhu sekarang di ");  logFile.print(temperature);  logFile.println("*C");

    digitalWrite(relayblower, HIGH);
    blow = "NYALA ";
    lcd.setCursor(0,3);     lcd.print("BLOWER");         lcd.setCursor(10, 3);  lcd.print(": "); lcd.print(blow);
  } 
  else{ 
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.print("Blower mati dengan suhu sekarang di ");  Serial.print(temperature);  Serial.println("*C");
    logFile.print(timeString);   logFile.print("  -->  ");  logFile.print("Blower mati dengan suhu sekarang di ");  logFile.print(temperature);  logFile.println("*C");
    digitalWrite(relayblower, LOW);
    blow = "MATI ";
    lcd.setCursor(0,3);     lcd.print("BLOWER");         lcd.setCursor(10, 3);  lcd.print(": "); lcd.print(blow);
  }
}

void controllingPH() {
  File logFile = SD.open("/relay_log.txt", FILE_APPEND);
  if (!logFile) {
    Serial.println("Failed to open file for writing");
    return;
  }

  // Tambahkan fungsi jam terlebih dahulu
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  strftime(timeStringBuff, sizeof(timeStringBuff), "%d %B %Y %H:%M:%S", &timeinfo);
  timeString = String(timeStringBuff);

  // masuk program controlling ph
  float intervalConPH = (((ph - ph_max) - 0.1887 + (0.0038 * jsn_max)) / 0.122) * 1000;
  if (ph > ph_max) {
    lcd.setCursor(0,0);     lcd.print("POMPA PH");      lcd.setCursor(10, 0);  lcd.print(": "); lcd.print("NYALA ");
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.print("Controlling ph nyala dengan nilai ph sekarang ");  Serial.print(ph);  Serial.print(" dengan interval "); Serial.print(intervalConPH);   Serial.println(" ms.");
    logFile.print(timeString);   logFile.print("  -->  ");  logFile.print("Controlling ph nyala dengan nilai ph sekarang ");  logFile.print(ph);  logFile.print(" dengan interval "); logFile.print(intervalConPH);   logFile.println(" ms.");
    digitalWrite(relayPh, HIGH);
    // esp_task_wdt_reset();
    delay(intervalConPH);
    // esp_task_wdt_reset();
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      return;
    }
    strftime(timeStringBuff, sizeof(timeStringBuff), "%d %B %Y %H:%M:%S", &timeinfo);
    timeString = String(timeStringBuff);
    digitalWrite(relayPh, LOW);
    pompaph = "MATI ";
    lcd.setCursor(0,0);     lcd.print("POMPA PH");      lcd.setCursor(10, 0);  lcd.print(": "); lcd.print(pompaph);
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.println("Controlling ph selesai. Mulai mematikan controlling ph");
    logFile.print(timeString);   logFile.print("  -->  ");  logFile.println("Controlling ph selesai. Mulai mematikan controlling ph");
  } 
  else{
    //esp_task_wdt_reset();
    pompaph = "MATI ";
    lcd.setCursor(0,0);     lcd.print("POMPA PH");      lcd.setCursor(10, 0);  lcd.print(": "); lcd.print(pompaph);
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.print("Controlling ph mati dengan ph sekarang ");  Serial.println(ph); 
    digitalWrite(relayPh, LOW);
  }
  logFile.close();

  // Verifikasi file
  logFile = SD.open("/relay_log.txt", FILE_READ);
  if (!logFile) {
      Serial.println("Failed to open file for verification");
      return;
  }

  bool isWritten = false;
  while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      if (line.indexOf(timeString) != -1) {
          isWritten = true;
          break;
      }
  }
}

void controllingTDSA() {
  File logFile = SD.open("/relay_log.txt", FILE_APPEND);
  if (!logFile) {
    Serial.println("Failed to open file for writing");
    return;
  }

  // Tambahkan fungsi jam terlebih dahulu
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  strftime(timeStringBuff, sizeof(timeStringBuff), "%d %B %Y %H:%M:%S", &timeinfo);
  timeString = String(timeStringBuff);

  // masuk ke program utama controlling nutrisi a
  float intervalConTDSA = ((((tds_max - tds) / 2) - 14.148 + (0.22564 * jsn_max)) / 3.312) * 1000;
  if (tds < tds_max) {
    pompanuta = "NYALA ";
    lcd.setCursor(0,1);     lcd.print("POMPA NUTA");   lcd.setCursor(10, 1);  lcd.print(": "); lcd.print(pompanuta);
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.print("Controlling Nutrisi A nyala dengan nilai TDS sekarang ");  Serial.print(tds);  Serial.print(" ppm. Dengan interval "); Serial.print(intervalConTDSA);   Serial.println(" ms.");
    logFile.print(timeString);   logFile.print("  -->  ");  logFile.print("Controlling Nutrisi A nyala dengan nilai TDS sekarang ");  logFile.print(tds);  logFile.print(" ppm. Dengan interval "); logFile.print(intervalConTDSA);   logFile.println(" ms.");
    digitalWrite(relayNutA, HIGH);
    //esp_task_wdt_reset();
    delay(intervalConTDSA);
    //esp_task_wdt_reset();
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      return;
    }
    strftime(timeStringBuff, sizeof(timeStringBuff), "%d %B %Y %H:%M:%S", &timeinfo);
    timeString = String(timeStringBuff);
    digitalWrite(relayNutA, LOW);
    pompanuta = "MATI ";
    lcd.setCursor(0,1);     lcd.print("POMPA NUTA");   lcd.setCursor(10, 1);  lcd.print(": "); lcd.print(pompanuta);
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.println("Controlling Nutrisi A selesai. Mulai mematikan controlling nutrisi A.");
    logFile.print(timeString);   logFile.print("  -->  ");  logFile.println("Controlling Nutrisi A selesai. Mulai mematikan controlling nutrisi A.");
  } 
  else {
    //esp_task_wdt_reset();
    pompanuta = "MATI ";
    digitalWrite(relayNutA, LOW);
    lcd.setCursor(0,1);     lcd.print("POMPA NUTA");   lcd.setCursor(10, 1);  lcd.print(": "); lcd.print(pompanuta);
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.print("Controlling Nutrisi A mati dengan nilai TDS sekarang ");  Serial.print(tds);  Serial.println(" ppm.");
  }
  logFile.close();
  
  // Verifikasi file
  logFile = SD.open("/relay_log.txt", FILE_READ);
  if (!logFile) {
      Serial.println("Failed to open file for verification");
      return;
  }

  bool isWritten = false;
  while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      if (line.indexOf(timeString) != -1) {
          isWritten = true;
          break;
      }
  }
}

void controllingTDSB() {
  File logFile = SD.open("/relay_log.txt", FILE_APPEND);
  if (!logFile) {
    Serial.println("Failed to open file for writing");
    return;
  }

   // Tambahkan fungsi jam terlebih dahulu
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  strftime(timeStringBuff, sizeof(timeStringBuff), "%d %B %Y %H:%M:%S", &timeinfo);
  timeString = String(timeStringBuff);

  // masuk ke program utama controlling nut b
  float intervalConTDSB = ((((tds_max - tds) / 2) - 9.019 + (0.1414 * jsn_max)) / 1.781) * 1000;
  if (tds < tds_max) {
    pompanutb = "NYALA ";
    lcd.setCursor(0,2);     lcd.print("POMPA NUTB");   lcd.setCursor(10, 2);  lcd.print(": "); lcd.print(pompanutb);
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.print("Controlling Nutrisi B nyala dengan nilai TDS sekarang ");  Serial.print(tds);  Serial.print(" ppm. Dengan interval "); Serial.print(intervalConTDSB);   Serial.println(" ms.");
    logFile.print(timeString);   logFile.print("  -->  ");  logFile.print("Controlling Nutrisi B nyala dengan nilai TDS sekarang ");  logFile.print(tds);  logFile.print(" ppm. Dengan interval "); logFile.print(intervalConTDSB);   logFile.println(" ms.");
    digitalWrite(relayNutB, HIGH);
    delay(intervalConTDSB);
    //esp_task_wdt_reset();
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      return;
    }
    strftime(timeStringBuff, sizeof(timeStringBuff), "%d %B %Y %H:%M:%S", &timeinfo);
    timeString = String(timeStringBuff);
    digitalWrite(relayNutB, LOW);
    pompanutb = "MATI ";
    lcd.setCursor(0,2);     lcd.print("POMPA NUTB");   lcd.setCursor(10, 2);  lcd.print(": "); lcd.print(pompanutb);
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.println("Controlling Nutrisi B selesai. Mulai mematikan controlling nutrisi B.");
    logFile.print(timeString);   logFile.print("  -->  ");  logFile.println("Controlling Nutrisi B selesai. Mulai mematikan controlling nutrisi B.");
  } 
  else if(tds > tds_max){
    //esp_task_wdt_reset();
    digitalWrite(relayNutB, LOW);
    pompanutb = "MATI ";
    lcd.setCursor(0,2);     lcd.print("POMPA NUT B");   lcd.setCursor(10, 2);  lcd.print(": "); lcd.print(pompanutb);
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.print("Controlling Nutrisi B mati dengan nilai TDS sekarang ");  Serial.print(tds);  Serial.println(" ppm.");
    logFile.print(timeString);   logFile.print("  -->  ");  logFile.print("Controlling Nutrisi B mati dengan nilai TDS sekarang ");  logFile.print(tds);  logFile.println(" ppm.");
  }
  logFile.close();

  
  // Verifikasi file
  logFile = SD.open("/relay_log.txt", FILE_READ);
  if (!logFile) {
      Serial.println("Failed to open file for verification");
      return;
  }

  bool isWritten = false;
  while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      if (line.indexOf(timeString) != -1) {
          isWritten = true;
          break;
      }
  }
}
