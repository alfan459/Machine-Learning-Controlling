/*!
 * @file DFRobot_PH_Test.h
 * @brief This is the sample code for Gravity: Analog pH Sensor / Meter Kit V2, SKU:SEN0161-V2.
 * @n In order to guarantee precision, a temperature sensor such as DS18B20 is needed, to execute automatic temperature compensation.
 * @n You can send commands in the serial monitor to execute the calibration.
 * @n Serial Commands:
 * @n    enterph -> enter the calibration mode
 * @n    calph   -> calibrate with the standard buffer solution, two buffer solutions(4.0 and 7.0) will be automaticlly recognized
 * @n    exitph  -> save the calibrated parameters and exit from calibration mode
 *
 * @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author [Jiawei Zhang](jiawei.zhang@dfrobot.com)
 * @version  V1.0
 * @date  2018-11-06
 * @url https://github.com/DFRobot/DFRobot_PH
 */

#include "DFRobot_PH.h"
#include <EEPROM.h>

#define PH_PIN A0
float voltage,phValue,temperature = 25;
DFRobot_PH ph;

int pHrelaypin = 31, ECrelaypin = 29, pomparelay = 27, h;

void setup()
{
    Serial.begin(115200);  
    // Relay
  pinMode(pHrelaypin, OUTPUT);  
  pinMode(ECrelaypin, OUTPUT);
  pinMode(pomparelay, OUTPUT);
    ph.begin();

}

void loop()
{
    static unsigned long timepoint = millis();
    if(millis()-timepoint>1000U){                  //time interval: 1s
        timepoint = millis();
        relay(1, 0, 0);
        //temperature = readTemperature();         // read your temperature sensor to execute temperature compensation
        voltage = analogRead(PH_PIN)/1024.0*5000;  // read the voltage
        phValue = ph.readPH(voltage,temperature);  // convert voltage to pH with temperature compensation
        Serial.print("temperature:");
        Serial.print(temperature,1);
        Serial.print("^C  pH:");
        Serial.print(phValue,2);
        float phbaru = phValue / 1.33;
        Serial.print("  Data baru: ");
        Serial.println(phbaru);

    }
    ph.calibration(voltage,temperature);           // calibration process by Serail CMD
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
