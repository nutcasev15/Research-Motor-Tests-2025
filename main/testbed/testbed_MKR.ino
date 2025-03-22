#include <SdFat.h>

#define ADC_PIN_1 A3
#define ADC_PIN_2 A4
#define INBUILT_LED 32

#define N_ENABLE 7
#define ACK 6

#define SAMPLE_RATE_HZ 10000000
#define SAMPLE_INTERVAL_US (1000000UL / SAMPLE_RATE_HZ)

#define BUFFER_SIZE 1
#define SD_CS SDCARD_SS_PIN
#define FILE_NAME "testlaun.dat"

SdFat sd;
File logFile;

enum STATE {
  STANDBY,
  ACQUIRE,
  FINISH,
  FAILURE
};
STATE currentState = STANDBY;

typedef struct {
  uint32_t timestamp_us;
  uint16_t adc1;
  uint16_t adc2;
} Sample;

Sample buffer1[BUFFER_SIZE];
Sample buffer2[BUFFER_SIZE];

volatile Sample* active_buffer = buffer1;
volatile int buffer_index = 0;
volatile bool buffer1_full = false;
volatile bool buffer2_full = false;

unsigned long last_sample_time = 0;
unsigned long start;

// ---------- State Transition ----------
void stateTransition() {
  switch (currentState) {
    case STANDBY:
      if (digitalRead(N_ENABLE) == LOW) {
        if (logFile.isOpen()) {
          digitalWrite(ACK, LOW);
          currentState = ACQUIRE;
          start = millis();
          Serial.println("CURRENT MKR STATE: ACQUIRE");
        } else {
          Serial.println("Failed to open log file.");
          currentState = FAILURE;
          Serial.println("CURRENT MKR STATE: FAILURE");
        }
      }
      break;

    case ACQUIRE:
    digitalWrite(INBUILT_LED, HIGH);
      if (millis() - start >= 50000) {
        currentState = FINISH;
      }
      break;
  }
}
// void stateTransition() {
//   static String inputString = "";

//   while (Serial.available()) {
//     char inChar = (char)Serial.read();
//     if (inChar == '\n') {
//       inputString.trim();

//       if (inputString.equalsIgnoreCase("ACQUIRE")) {
//         currentState = ACQUIRE;
//         Serial.println("STATE: ACQUIRE");
//       } else if (inputString.equalsIgnoreCase("DONE")) {
//         currentState = FINISH;
//         Serial.println("STATE: FINISH");
//       } else {
//         Serial.print("Unknown command: ");
//         Serial.println(inputString);
//       }

//       inputString = "";
//     } else {
//       inputString += inChar;
//     }
//   }
// }

void logBuffer(Sample* buf) {
  logFile.write((uint8_t*)buf, sizeof(Sample) * BUFFER_SIZE);
  // Serial.println("Logged data :)");
  logFile.flush();
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  analogReadResolution(12);

  if (!sd.begin(SD_CS)) {
    Serial.println("SD init failed!");
    currentState = FAILURE;
    return;
  }

  logFile = sd.open(FILE_NAME, O_CREAT | O_WRITE | O_TRUNC);
  if (!logFile) {
    Serial.println("Failed to open log file!");
    currentState = FAILURE;
    return;
  }

  // Optional: preallocate 10MB for speed
  // logFile.preAllocate(10UL * 1024 * 1024);

  Serial.println("System ready. Send 'ACQUIRE' to start logging.");
}

void loop() {
  stateTransition();

  if (currentState == ACQUIRE) {
    unsigned long now = micros();

    // if (now - last_sample_time >= SAMPLE_INTERVAL_US) {
      last_sample_time = now;

      Sample s;
      s.timestamp_us = now;
      s.adc1 = analogRead(ADC_PIN_1);
      delayMicroseconds(3);
      s.adc2 = analogRead(ADC_PIN_2);

      Sample* buf = (Sample*)active_buffer;
      buf[buffer_index++] = s;

      if (buffer_index >= BUFFER_SIZE) {
        if (active_buffer == buffer1) {
          buffer1_full = true;
          active_buffer = buffer2;
        } else {
          buffer2_full = true;
          active_buffer = buffer1;
        }
        buffer_index = 0;
      }
    // }

    if (buffer1_full) {
      buffer1_full = false;
      logBuffer(buffer1);
    }

    if (buffer2_full) {
      buffer2_full = false;
      logBuffer(buffer2);
    }
  }

  else if (currentState == FINISH) {
    Serial.println("Finalizing log file...");
    if (buffer_index > 0) {
      logBuffer((Sample*)active_buffer);  // Dump remaining samples
    }
    logFile.close();
    Serial.println("Logging complete.");
    while (1);
  }

  else if (currentState == FAILURE) {
    logFile.close();
    Serial.println("SYSTEM FAILURE. Logging halted.");
    while (1);
  }
}
