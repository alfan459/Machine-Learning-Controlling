#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "esp_task_wdt.h"
#include "time.h"
#include "Wire.h"
#include <LiquidCrystal_I2C.h>
#include "WeatherClient.h"
#include "ControllingClient.h"
#include <WiFiManager.h>

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 0;
char timeStringBuff[30];
String timeString;
struct tm timeinfo;
int hour, minutes;

#define LED 2

const char* serverUrlWeather = "https://hydroponic.omahiot.com/api/getNutrientWeather/3";
const char* serverUrlControlling = "https://hydroponic.omahiot.com/api/getControlling?gh_id=3";

ControllingClient controllingClient(serverUrlControlling);
WeatherClient weatherClient(serverUrlWeather);

float ph_max, tds_max, tandon_max, suhu_max, temperature, humidity, light, ph, tds, tandon, suhu_air;
String waktu;

// inisialisasi variable toleransi
float ph_tol_min, ph_tol_max;
int tds_tol_min, tds_tol_max;

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

  WiFiManager wm;
  bool res;
  res = wm.autoConnect("Controlling","Controlling 123"); // password protected ap
  if(!res) {
      Serial.println("Failed to connect");
      // ESP.restart();
  } 
  else {
      //if you get here you have connected to the WiFi    
      Serial.println("connected...yeey :)");
  }

  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }

  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  lcd.setCursor(0,0);  lcd.print("WiFi Connected");
  delay(5000);
  lcd.clear();

  // Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");

  // esp_task_wdt_init(8, true);
  // esp_task_wdt_add(NULL);

  // Menunggu sinkronisasi waktu
  
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "Current time: %Y-%m-%d %H:%M:%S");
  createFileIfNotExists(); 
}

void loop() {
  //esp_task_wdt_reset();

  hour = timeinfo.tm_hour;
  minutes = timeinfo.tm_min;

  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  
  if(WiFi.status() == WL_CONNECTED){
    digitalWrite(LED, HIGH);
  }
  else{
    digitalWrite(LED, LOW);
    ESP.restart();
  }

  if(hour >= 7 && hour <= 17){
    if (WiFi.status() == WL_CONNECTED) {
      if(minutes % 13 ==0){
        getDataFromWeatherAPI();
        blower();
      }

      if(hour == 7 && minutes % 13 == 0){
        getDataFromWeatherAPI();
        getDataFromControllingAPI();
        lcd.clear();
        controllingPH();
        controllingTDSA();
        controllingTDSB();
      }

      if(hour == 16 && minutes % 13 == 0){
        getDataFromWeatherAPI();
        getDataFromControllingAPI();
        lcd.clear();
        controllingPH();
        controllingTDSA();
        controllingTDSB();
      }
    }
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
  if (weatherClient.fetchData()) {
    Serial.println("Data fetched successfully!");
    waktu = weatherClient.getServerTime();
    temperature = weatherClient.getTemperature();
    humidity = weatherClient.getHumidity();
    light = weatherClient.getLight();
    ph = weatherClient.getPH();
    tds = weatherClient.getTDS();
    tandon = weatherClient.getNutrientLevel();
    suhu_air = weatherClient.getWaterTemperature();
  } else {
    Serial.println("Failed to fetch data.");
  }
}

void getDataFromControllingAPI() {
  if (controllingClient.fetchData()) {
    Serial.println("Data fetched successfully!");
    suhu_max = controllingClient.getSuhuMax();
    ph_max = controllingClient.getPhMax();
    tds_max = controllingClient.getTdsMax();
  } else {
    Serial.println("Failed to fetch data.");
  }
}

void blower(){
  File logFile = SD.open("/relay_log.txt", FILE_APPEND);
  if (!logFile) {
    Serial.println("Failed to open file for writing");
    return;
  }

  // Tambahkan fungsi jam terlebih dahulu
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
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  strftime(timeStringBuff, sizeof(timeStringBuff), "%d %B %Y %H:%M:%S", &timeinfo);
  timeString = String(timeStringBuff);

  // nilai toleransi
  ph_tol_min = ph_max - 0.2;
  ph_tol_max = ph_max + 0.4;

  // masuk program controlling ph
  float intervalConPH = ((0.0440610566908398 * tandon) + (10.4878257572978 * (ph - ph_max))  -2.25033477638771) * 1000;
  if (ph_tol_min < ph && ph_tol_max < ph) {
    lcd.setCursor(0,0);         lcd.print("POMPA PH");      lcd.setCursor(10, 0);  lcd.print(": "); lcd.print("NYALA ");
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.print("Controlling ph nyala dengan nilai ph sekarang ");  Serial.print(ph);  Serial.print(" dengan interval "); Serial.print(intervalConPH);   Serial.println(" ms.");
    logFile.print(timeString);   logFile.print("  -->  ");  logFile.print("Controlling ph nyala dengan nilai ph sekarang ");  logFile.print(ph);  logFile.print(" dengan interval "); logFile.print(intervalConPH);   logFile.println(" ms.");
    digitalWrite(relayPh, HIGH);
    // esp_task_wdt_reset();
    delay(intervalConPH);
    // esp_task_wdt_reset();
    
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
  
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  strftime(timeStringBuff, sizeof(timeStringBuff), "%d %B %Y %H:%M:%S", &timeinfo);
  timeString = String(timeStringBuff);

  // Nilai Toleransi
  tds_tol_min = tds_max - 50;
  tds_tol_max = tds_max + 50;

  // masuk ke program utama controlling nutrisi a
  float intervalConTDSA = (((0.350314869129155 * tandon) + (0.257992452699358 * (tds_max - tds)) - 20.3384994785302) * 0.7) * 1000;
  if (tds_tol_min > tds && tds_tol_max > tds) {
    pompanuta = "NYALA ";
    lcd.setCursor(0,1);     lcd.print("POMPA NUTA");   lcd.setCursor(10, 1);  lcd.print(": "); lcd.print(pompanuta);
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.print("Controlling Nutrisi A nyala dengan nilai TDS sekarang ");  Serial.print(tds);  Serial.print(" ppm. Dengan interval "); Serial.print(intervalConTDSA);   Serial.println(" ms.");
    logFile.print(timeString);   logFile.print("  -->  ");  logFile.print("Controlling Nutrisi A nyala dengan nilai TDS sekarang ");  logFile.print(tds);  logFile.print(" ppm. Dengan interval "); logFile.print(intervalConTDSA);   logFile.println(" ms.");
    digitalWrite(relayNutA, HIGH);
    //esp_task_wdt_reset();
    delay(intervalConTDSA);
    //esp_task_wdt_reset();
    
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
  
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  strftime(timeStringBuff, sizeof(timeStringBuff), "%d %B %Y %H:%M:%S", &timeinfo);
  timeString = String(timeStringBuff);

  // Nilai Toleransi
  tds_tol_min = tds_max - 50;
  tds_tol_max = tds_max + 50;

  // masuk ke program utama controlling nut b
  float intervalConTDSB = (((0.398119334838059 * tandon) + (0.525005800525129 * (tds_max - tds)) - 19.5362441144985) * 0.3) * 1000;
  if (tds_tol_min > tds && tds_tol_max > tds) {
    pompanutb = "NYALA ";
    lcd.setCursor(0,2);     lcd.print("POMPA NUTB");   lcd.setCursor(10, 2);  lcd.print(": "); lcd.print(pompanutb);
    Serial.print(timeString);   Serial.print("  -->  ");  Serial.print("Controlling Nutrisi B nyala dengan nilai TDS sekarang ");  Serial.print(tds);  Serial.print(" ppm. Dengan interval "); Serial.print(intervalConTDSB);   Serial.println(" ms.");
    logFile.print(timeString);   logFile.print("  -->  ");  logFile.print("Controlling Nutrisi B nyala dengan nilai TDS sekarang ");  logFile.print(tds);  logFile.print(" ppm. Dengan interval "); logFile.print(intervalConTDSB);   logFile.println(" ms.");
    digitalWrite(relayNutB, HIGH);
    delay(intervalConTDSB);
    //esp_task_wdt_reset();
    
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
