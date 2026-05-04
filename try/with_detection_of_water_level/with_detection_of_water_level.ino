// Sensor pins
const int sensorPins[5] = {A0, A1, A2, A3, A4};
const int WET_THRESHOLD = 650;

void setup() {
  Serial.begin(9600);
  Serial.println("Water Level Detection System");
  Serial.println("--------------------------------");
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
  int waterLevelSensor = -1; // -1 means no water detected

  for (int i = 0; i < 5; i++) { // bottom to top
    if (sensorWet[i]) {
      waterLevelSensor = i; // update water level
    } else {
      // If a lower sensor is dry, no higher sensor should be wet
      for (int j = i+1; j < 5; j++) {
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

  // Print water level status
  if (valid) {
    if (waterLevelSensor == -1) {
      Serial.println("Water level: NONE");
    } else {
      Serial.print("Water level at sensor: A");
      Serial.println(waterLevelSensor);
    }
  } else {
    Serial.println("Water level status: INCONSISTENT (check sensors!)");
  }

  Serial.println("--------------------------------");
  delay(1000);
}
