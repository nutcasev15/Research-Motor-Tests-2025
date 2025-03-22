//example code from Adafruit, but on pins 10-13 and for two SPI thermocouples on with CS
//MAX6675 thermocouple modules

#include "max6675.h"

int thermoDO = 12;
int thermoCS1 = 10;
int thermoCS2 = 11;
int thermoCLK = 13;

MAX6675 thermocouple1(thermoCLK, thermoCS1, thermoDO);
MAX6675 thermocouple2(thermoCLK, thermoCS2, thermoDO);

void setup() {
  Serial.begin(9600);

  Serial.println("MAX6675 test");
  // wait for MAX chip to stabilize
  delay(500);
}

void loop() { 
   Serial.print("Thermocouple 1 in Celsius= "); 
   Serial.println(thermocouple1.readCelsius());
   
   Serial.print("Thermocouple 2 in Celsius = "); 
   Serial.println(thermocouple2.readCelsius());
   // For the MAX6675 to update, you must delay AT LEAST 250ms between reads!
   delay(1000);
}
