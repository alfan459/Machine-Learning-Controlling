#include "time.h"

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

void setup(){
	Serial.begin(9600);
	configTime(gmtOffset, daylightOffset, ntpServer);
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo)){
		Serial.println("Gagal untuk mendapatkan data terbaru");
		return;
	}
	Serial.println(&timeinfo, "Current time: %Y-%m%d %H:%M:%S");
}

void loop(){
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo)){
		Serial.println("Gagal untuk mendapatkan data terbaru");
		return;
	}
	
	delay(1000);

	int hour = timeinfo.tm_hour;
	
	if(hour == 7 || hour == 8){
		// program yang perlu dijalankan
	}

	else if(hour == 15 || hour == 16){
		// program yang perlu dijalankan
	}

	else{
		// buat program agar tidak menjalankan apapun
	}
}
