#define PRESSURE    A3
#define ANALOG_RES  12

void setup () {
    Serial.begin(115200);
    while(!Serial);

    pinMode(PRESSURE, INPUT);
    analogReadResolution(ANALOG_RES);

    Serial.println("\nPressure Transducer Test");
}

void loop () {
    int adc = analogRead(PRESSURE);
    float pres = (250.0/4.0) * (((float)adc/(1 << ANALOG_RES - 1)) * 3.3 - 0.5);

    Serial.print("Gauge Pressure = ");
    Serial.print(pres);
    Serial.println(" bar");

    delay(100);
}