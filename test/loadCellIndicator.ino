#define LOADCELL    A4
#define ANALOG_RES  12

#define MAX_READ    450
#define G           9.80665

void setup () {
    Serial.begin(115200);
    while(!Serial);

    pinMode(LOADCELL, INPUT);
    analogReadResolution(ANALOG_RES);

    Serial.println("\nLoad Cell Test");
}

void loop () {
    int adc = analogRead(LOADCELL);
    float volt = ((float)adc/(1 << ANALOG_RES - 1)) * 3.3;
    float load = MAX_READ * (volt/10.0) * G;

    Serial.print("Current Thrust = ");
    Serial.print(load);
    Serial.println(" N");

    delay(100);
}