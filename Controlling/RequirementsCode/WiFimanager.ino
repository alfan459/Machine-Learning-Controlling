#include <WiFiManager.h>

void setup(){
	Serial.begin(115200);
	WiFiManager wifimanager;
	bool res;
	res = wm.autoConnect("AccessPoint","password");
	if(!res){
		Serial.println("Failed to connect");
		ESP.restart();
	}
	else{
		Serial.println("Connected to WiFi");
	}
}

void loop(){

}
