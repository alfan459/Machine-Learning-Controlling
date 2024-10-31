/*
 * file DFRobot_EC.ino
 * @ https://github.com/DFRobot/DFRobot_EC
 *
 * This is the sample code for Gravity: Analog Electrical Conductivity Sensor / Meter Kit V2 (K=1.0), SKU: DFR0300.
 * In order to guarantee precision, a temperature sensor such as DS18B20 is needed, to execute automatic temperature compensation.
 * You can send commands in the serial monitor to execute the calibration.
 * Serial Commands:
 *   enterec -> enter the calibration mode
 *   calec -> calibrate with the standard buffer solution, two buffer solutions(1413us/cm and 12.88ms/cm) will be automaticlly recognized
 *   exitec -> save the calibrated parameters and exit from calibration mode
 *
 * Copyright   [DFRobot](http://www.dfrobot.com), 2018
 * Copyright   GNU Lesser General Public License
 *
 * version  V1.0
 * date  2018-03-21
 */

#include "DFRobot_EC.h"
#include <EEPROM.h>

#define EC_PIN A2
float voltage,ecValue,temperature = 25;
DFRobot_EC ec;

int pHrelaypin = 31, ECrelaypin = 29, pomparelay = 27, h;

void setup()
{
  Serial.begin(115200);  
  // Relay
  pinMode(pHrelaypin, OUTPUT);  
  pinMode(ECrelaypin, OUTPUT);
  pinMode(pomparelay, OUTPUT);
  ec.begin();
}

void loop()
{
    static unsigned long timepoint = millis();
    if(millis()-timepoint>1000U)  //time interval: 1s
    {
      relay(0, 1, 0);
      timepoint = millis();
      voltage = analogRead(EC_PIN)/1024.0*5000;   // read the voltage
      //temperature = readTemperature();          // read your temperature sensor to execute temperature compensation
      ecValue =  ec.readEC(voltage,temperature);  // convert voltage to EC with temperature compensation
      Serial.print("temperature:");
      Serial.print(temperature,1);
      Serial.print("^C  EC:");
      Serial.print(ecValue,2);
      Serial.print("ms/cm");
      int tds = ecValue*643;
      Serial.print("    Nilai TDS: ");
      Serial.println(tds);
    }
    ec.calibration(voltage,temperature);          // calibration process by Serail CMD
}

void relay(int pH_state, int EC_state, int pompa_state) {
  digitalWrite(pHrelaypin, pH_state);
  digitalWrite(ECrelaypin, EC_state);
  digitalWrite(pomparelay, pompa_state);
}

float readTemperature()
{
  //add your code here to get the temperature from your temperature sensor
}
