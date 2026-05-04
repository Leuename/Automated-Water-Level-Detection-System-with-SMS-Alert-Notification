#include <SoftwareSerial.h>

// GSM module pins
SoftwareSerial SIM900A(10, 11); // RX, TX
const char phoneNumber[] = "+63XXXXXXXXXX"; // Replace with your number

// Water sensor pins
const int sensorPins[5] = {A0, A1, A2, A3, A4};
const int WET_THRESHOLD = 630;

// Track last water level for alerts
int lastAlertLevel = -1;
int currentLevel = -1;
unsigned long levelStartTime = 0;
const unsigned long CONFIRM_DELAY = 5000; // 5 seconds

void setup() {
  Serial.begin(9600);
  SIM900A.begin(9600);

  Serial.println("--- SYSTEM ONLINE: WATER LEVEL GSM ALERT ---");
}

void loop() {
  int sensorValues[5];
  bool sensorWet[5];

  // Read sensors and determine wet/dry
  for (int i = 0; i < 5; i++) {
    sensorValues[i] = analogRead(sensorPins[i]);
    sensorWet[i] = sensorValues[i] >= WET_THRESHOLD;
  }

  // Check consistency and determine water level
  bool valid = true;
  int waterLevelSensor = -1;

  for (int i = 0; i < 5; i++) { // bottom to top
    if (sensorWet[i]) {
      waterLevelSensor = i; // update water level
    } else {
      for (int j = i + 1; j < 5; j++) {
        if (sensorWet[j]) {
          valid = false;
          break;
        }
      }
      if (!valid) break;
    }
  }

  // Print sensor readings
  for (int i = 0; i < 5; i++) {
    Serial.print("A");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(sensorValues[i]);
    Serial.print(" -> ");
    Serial.println(sensorWet[i] ? "WET" : "DRY");
  }

  // Handle water level logic
  if (valid) {
    if (waterLevelSensor == -1) {
      Serial.println("Water level: NONE");
    } else {
      Serial.print("Water level at sensor: A");
      Serial.println(waterLevelSensor);
    }

    // If the detected level has changed, start timer
    if (waterLevelSensor != currentLevel) {
      currentLevel = waterLevelSensor;
      levelStartTime = millis();
    } else {
      // If level stayed for 5 seconds, send alert
      if (millis() - levelStartTime >= CONFIRM_DELAY) {
        if (currentLevel > lastAlertLevel) {
          sendSMSRise(currentLevel);
          lastAlertLevel = currentLevel;
        } else if (currentLevel < lastAlertLevel) {
          sendSMSDrop(currentLevel);
          lastAlertLevel = currentLevel;
        }
        levelStartTime = millis(); // reset timer to avoid repeat SMS
      }
    }

  } else {
    Serial.println("Water level status: INCONSISTENT (check sensors!)");
  }

  Serial.println("--------------------------------");
  delay(500); // small delay for loop stability
}

// Send SMS for water rising
void sendSMSRise(int sensor) {
  Serial.print("Sending RISE SMS... ");
  SIM900A.print("AT+CMGF=1\r"); // text mode
  delay(100);
  SIM900A.print("AT+CMGS=\"");
  SIM900A.print(phoneNumber);
  SIM900A.print("\"\r");
  delay(100);

  SIM900A.print("ALERT! Water level RAISED to sensor A");
  SIM900A.print(sensor);
  SIM900A.print(".");
  delay(100);

  SIM900A.write(26); // Ctrl+Z to send
  delay(5000);
  Serial.println("SMS SENT.");
}

// Send SMS for water dropping
void sendSMSDrop(int sensor) {
  Serial.print("Sending DROP SMS... ");
  SIM900A.print("AT+CMGF=1\r"); // text mode
  delay(100);
  SIM900A.print("AT+CMGS=\"");
  SIM900A.print(phoneNumber);
  SIM900A.print("\"\r");
  delay(100);

  if (sensor == -1) {
    SIM900A.print("ALERT! Water fully DROPPED.");
  } else {
    SIM900A.print("ALERT! Water level DROPPED to sensor A");
    SIM900A.print(sensor);
    SIM900A.print(".");
  }
  delay(100);

  SIM900A.write(26); // Ctrl+Z to send
  delay(5000);
  Serial.println("SMS SENT.");
}
