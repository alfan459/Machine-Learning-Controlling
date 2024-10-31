#include <avr/wdt.h>

#define NUM_READINGS 10
int thisReading = 0;

//DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>          // Library yang digunakan untuk DS18B20
#define DS 10                           // pin data ds18B20
OneWire oneWire(DS);
DallasTemperature sensors(&oneWire);
float temperatureds;                    // temperatureds => variabel pembacaan suhu air

//EC
#include "DFRobot_EC.h"                 // library yang digunakan untuk sensor EC
#include <EEPROM.h>
#define EC_PIN A2                       // pin data sensor EC terhubung ke A2
float tds_val, ecValue, voltageec, totalEC, averageEC, readingsEC[NUM_READINGS];
int readIndexEC = 0;
DFRobot_EC ec;
long tds_val2;
#define KVALUEADDR 0x0F

//PH
#include "DFRobot_PH.h"                       // library sensor pH
#define PH_PIN A0                             // pin data terhubung ke pin A0
float voltageph, phValue, totalPH, averagePH, readingsPH[NUM_READINGS], phbaru;
int readIndexPH = 0;
DFRobot_PH ph;

//A02YYUW
#include <SoftwareSerial.h>
// Define connections to sensor
int pinRX = 12;
int pinTX = 11;
unsigned char data_buffer[4] = {0};
int tinggi = 35, panjang = 68, lebar = 40, distance = 0, tinggi2;
int long volume, level_air;
unsigned char CS;
SoftwareSerial mySerial(pinRX, pinTX);

//LCD
#include <Wire.h>                               
#include <LiquidCrystal_I2C.h>                  // library i2c lcd
#define SDA_PIN 20                              // sda 20
#define SCL_PIN 21                              // scl 21    
LiquidCrystal_I2C lcd(0x27, 20, 4);             // alamat lcd 0x27

//RTC
#include <RtcDS3231.h>                          // library RTCDS3231
RtcDS3231<TwoWire> Rtc(Wire);
String datetime;                                // untuk tanggal
String timeonly;                                // untuk waktu
int thn, bln, tgl, jam, mnt, dtk;    

String kirim;
int pHrelaypin = 31, ECrelaypin = 29, pomparelay = 27, h;

// State machine
enum State{
  IDLE,
  SAMPLING_SUHU,
  SAMPLING_PH,
  SAMPLING_EC,
  KIRIM_DATA
};

State currentState = IDLE;    // posisi sekarang idle


//Timer
unsigned long lastTime = 0;
unsigned long minutes = 0;
unsigned long hours = 0;

#define countof(a) (sizeof(a) / sizeof(a[0]))
void printDateTime(const RtcDateTime& dt) {
  char datestring[20];
  char timestring[20];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%04u-%02u-%02u"),
             dt.Year(),
             dt.Month(),
             dt.Day(),

             dt.Hour(),
             dt.Minute(),
             dt.Second());
  // Serial.print(datestring);
  snprintf_P(timestring,
             countof(timestring),
             PSTR("%02u:%02u:%02u"),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  // Serial.print(timestring);
  datetime = datestring;
  timeonly = timestring;
}

void setup() {
  Serial.begin(9600);
  Serial3.begin(115200);
  mySerial.begin(9600);
  
  Wire.begin();         
  Wire.setClock(10000);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();

  // inisialisasi sensor sensor
  sensors.begin();                      // DS18B20
  ec.begin();                           // EC
  for (byte i = 0; i < 8; i++) {
    EEPROM.write(KVALUEADDR + i, 0xFF);
  }
  ph.begin();                           // pH


  Rtc.Begin();
  #if defined(WIRE_HAS_TIMEOUT)
    Wire.setWireTimeout(3000 /* us /, true / reset_on_timeout */);
  #endif

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);

  if (!Rtc.IsDateTimeValid()) {
      Serial.println("RTC lost confidence in the DateTime!");
      Rtc.SetDateTime(compiled);
  }

  if (!Rtc.GetIsRunning()) {
    
      Serial.println("RTC was not actively running, starting now");
      Rtc.SetIsRunning(true);
    
  }

  RtcDateTime now = Rtc.GetDateTime();
  
    if (now < compiled) {
      Serial.println("RTC is older than compile time!  (Updating DateTime)");
      Rtc.SetDateTime(compiled);
    } else if (now > compiled) {
      Serial.println("RTC is newer than compile time. (this is expected)");
    } else if (now == compiled) {
      Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    }
  
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
  delay(250);
  
  // Relay
  pinMode(pHrelaypin, OUTPUT);  
  pinMode(ECrelaypin, OUTPUT);
  pinMode(pomparelay, OUTPUT);
  
  // Matikan semua relay, aktif jika LOW
  relay(1, 1, 1);
  
  waktuku();
  homeDisplay();
  for (thisReading = 0; thisReading < NUM_READINGS; thisReading++) {
    readingsPH[thisReading] = 0;
    readingsEC[thisReading] = 0;
  }
  wdt_enable(WDTO_8S);
}

void loop() {
  wdt_reset();
   switch (currentState) {
    case IDLE:
      wdt_reset();
      waktuku();
      displayLcd();
      
      relay(1, 1, 1);

      if (millis() - lastTime > 60000) {
        minutes++;
        lastTime = millis();
      }

      if (minutes == 10) {currentState = SAMPLING_SUHU;}
    break;

    case SAMPLING_SUHU:
      for (h=1; h <= 5; h++){
        // waktuku();
        wdt_reset();
        jarak();
        displayLcd();
      }
      for (h=1; h <= 20; h++){
        // waktuku();
        wdt_reset();
        relay(1,1,0);
        DSTEMP();
        displayLcd();
      }
      currentState = SAMPLING_PH;
    break;

    case SAMPLING_PH:
      for (h = 0; h <= 25; h++) {
        wdt_reset();
        relay(1, 0, 0);                                   
        totalPH = totalPH - readingsPH[readIndexPH];           // Mengurangi pembacaan terakhir dari total
        voltageph = analogRead(PH_PIN) / 1024.0 * 5000;  // Membaca dari sensor
        readingsPH[readIndexPH] = voltageph;                 // Menyimpan pembacaan baru dalam array
        totalPH = totalPH + readingsPH[readIndexPH];           // Menambahkan pembacaan baru ke total
        readIndexPH = readIndexPH + 1;                     // Maju ke posisi berikutnya dalam array

        // Jika kita sudah di akhir array, kembali ke awal
        if (readIndexPH >= NUM_READINGS) {
          readIndexPH = 0;
        }
        averagePH = totalPH / NUM_READINGS;             // Menghitung rata-rata
        phValue = ph.readPH(averagePH, temperatureds);  // Konversi tegangan rata-rata ke pH dengan kompensasi suhu
        // waktuku();
        displayLcd();
        delay(1000);
      }
      for (thisReading = 0; thisReading < NUM_READINGS; thisReading++) {
        readingsPH[thisReading] = 0;
      }
      totalPH = 0; readIndexPH = 0;
      currentState = SAMPLING_EC;
      phbaru = phValue * 1.33;
    break;

    case SAMPLING_EC:
      lcd.clear();
      for (h = 0; h <= 30; h++) {
        wdt_reset();
        relay(0, 1, 0);
        totalEC = totalEC - readingsEC[readIndexEC];           // Mengurangi pembacaan terakhir dari total
        voltageec = analogRead(EC_PIN) / 1024.0 * 5000;  // Membaca dari sensor
        readingsEC[readIndexEC] = voltageec;                 // Menyimpan pembacaan baru dalam array
        totalEC = totalEC + readingsEC[readIndexEC];           // Menambahkan pembacaan baru ke total
        readIndexEC = readIndexEC + 1;                     // Maju ke posisi berikutnya dalam array

        // Jika kita sudah di akhir array, kembali ke awal
        if (readIndexEC >= NUM_READINGS) {
          readIndexEC = 0;
        }
        averageEC = totalEC / NUM_READINGS;             // Menghitung rata-rata
        ecValue = ec.readEC(averageEC, temperatureds);  // Konversi tegangan rata-rata ke pH dengan kompensasi suhu
        tds_val = ecValue * 643;
        // waktuku();
        displayLcd();
        delay(1000);
      }
      for (thisReading = 0; thisReading < NUM_READINGS; thisReading++) {
        readingsEC[thisReading] = 0;
        totalEC = 0; readIndexEC = 0;
        tds_val2 = tds_val+0.5;

      }
      currentState = KIRIM_DATA;
    break;

    case KIRIM_DATA:
      wdt_reset();
      resetLCD();
      kirim_data();       // Kirim data ke esp8266
      relay(1, 1, 1);     // Matikan semua relay
      minutes = 0;        // Reset nilai menit ke 0
      currentState = IDLE;
    break;
  }
}

//--------------------All functions-------------------//
void waktuku() {
   if (!Rtc.IsDateTimeValid()) {
    if (Rtc.LastError() != 0) {
      Serial.print("RTC communications error = ");
      Serial.println(Rtc.LastError());
    } else {      Serial.println("RTC lost confidence in the DateTime!");    }
  }

  RtcDateTime now = Rtc.GetDateTime();
  printDateTime(now);
  RtcTemperature temp = Rtc.GetTemperature();
}

void DSTEMP() {
  sensors.requestTemperatures();                    // Perintah untuk mendapatkan suhu
  temperatureds = sensors.getTempCByIndex(0);       // gunakan function ByIndex sebagai index pembacaan sensor
  delay(1000);
}

void jarak() {
 if (mySerial.available() > 0) {
    delay(4);
    // Check for packet header character 0xff
    if (mySerial.read() == 0xff) {
      // Insert header into array
      data_buffer[0] = 0xff;
      // Read remaining 3 characters of data and insert into array
      for (int i = 1; i < 4; i++) {
        data_buffer[i] = mySerial.read();
      }

      //Compute checksum
      CS = data_buffer[0] + data_buffer[1] + data_buffer[2];
      // If checksum is valid compose distance from data
      if (data_buffer[3] == CS) {
        distance = (data_buffer[1] << 8) + data_buffer[2];
        int distancecm = distance / 10;
        tinggi2 = tinggi - distancecm;
      }
    }
  }
  volume = 68 * 40;
  level_air = volume * tinggi2 / 1000;
}

void kirim_data() {
  kirim = "";
  kirim += phbaru;         kirim += "!";
  kirim += tds_val2;        kirim += "@";
  kirim += temperatureds;   kirim += "#";
  kirim += level_air;       kirim += "$";
  Serial3.print(kirim);
  Serial.print(kirim);
}

void resetLCD(){
  lcd.clear();
  lcd.display();
  lcd.init();
  lcd.backlight();
}

void homeDisplay() {
  lcd.setCursor(7, 0);      lcd.print("COMYC");
  lcd.setCursor(5, 1);      lcd.print("TURUS ASRI");
  lcd.setCursor(0, 2);      lcd.print(datetime);
  lcd.setCursor(11, 2);     lcd.print(timeonly);
  delay(2000);
  lcd.clear();
}

void displayLcd() {
  lcd.setCursor(0, 0);    lcd.print(datetime);
  lcd.setCursor(11, 0);   lcd.print(timeonly);
  lcd.setCursor(0, 2);    lcd.print("pH: ");    lcd.print(phbaru);
  lcd.setCursor(9, 2);    lcd.print("EC: ");    lcd.print(tds_val2);      
  lcd.setCursor(0, 3);    lcd.print("WL: ");    lcd.print(level_air);   lcd.print(" L");   
  lcd.setCursor(9, 3);    lcd.print("WT: ");    lcd.print(temperatureds);
}

void relay(int pH_state, int EC_state, int pompa_state) {
  digitalWrite(pHrelaypin, pH_state);
  digitalWrite(ECrelaypin, EC_state);
  digitalWrite(pomparelay, pompa_state);
}
