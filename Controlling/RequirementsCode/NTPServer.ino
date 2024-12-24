# include "time.h"

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7*3600;
const int daylightOffset_sec = 0;

void setup(){
	Serial.begin(9600);
	
	// inisialisasi waktu
	configTime(gmtOffset, daylightOffset, ntpServer);
	printLocalTime();
}

void loop(){
	printLocalTime();
	delay(1000);
}

void printLocalTime(){
	struct tm timeinfo;
	if(!getLocalTime(&timeinfo)){
		Serial.println("Failed to obtain time");
		return;
	}
	Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
	Serial.print("Day of week: ");
	Serial.println(&timeinfo, "%A");
	
	Serial.print("Month: ");
	Serial.println(&timeinfo, "%B");

	Serial.print("Day of Month: ");
	Serial.println(&timeinfo, "%d");

	Serial.print("Year: ");
	Serial.println(&timeinfo, "%Y");

	Serial.print("Hour: ");
	Serial.println(&timeinfo, "%H");

	Serial.print("Hour (12 hour format): ");
	Serial.println(&timeinfo, "%I");

	Serial.print("Minute: ");
	Serial.println(&timeinfo, "%M");

	Serial.print("Second: ");
	Serial.println(&timeinfo, "%S");

	Serial.println("Time Variables");
	char timeHour[3];
	strftime(timeHour, 3, "%H", &timeinfo);
	Serial.println(timeHour);

	char timeWeekDay[10];
	strftime(timeHour, 10, "%A", &timeinfo);
	Serial.println(timeWeekDay);
}
