#include <SoftwareSerial.h>

// GSM module pins
SoftwareSerial SIM900A(10, 11); // RX, TX

// Multiple phone numbers - add as many as you need
const char* phoneNumbers[] = {
  "+639953632031",  // Person 1
  "+639285323563",  // Person 2
  "+639652559588",  // Person 3
  // Add more numbers here
};
const int numPhones = sizeof(phoneNumbers) / sizeof(phoneNumbers[0]);

// Water sensor pins (1ft to 5ft)
const int sensorPins[5] = {A0, A1, A2, A3, A4};
const int WET_THRESHOLD = 630;

// Sensor heights in feet
const int sensorHeights[5] = {1, 2, 3, 4, 5}; // A0=1ft, A1=2ft, A2=3ft, A3=4ft, A4=5ft

// Track last water level for alerts
int lastAlertLevel = -1;
int stableWaterLevel = -1;
unsigned long levelStartTime = 0;
const unsigned long CONFIRM_DELAY = 5000;

// Track sensor error state
bool lastSensorError = false;
bool stableSensorError = false;
unsigned long errorStartTime = 0;

void setup() {
  Serial.begin(9600);
  SIM900A.begin(9600);

  Serial.println("--- WATER LEVEL MONITORING SYSTEM (1-5 FEET) ---");
  Serial.print("Number of recipients: ");
  Serial.println(numPhones);
  
  // Initialize GSM module
  delay(5000); // Give module time to boot
  Serial.println("Initializing GSM...");
  
  // Test AT commands
  sendATCommand("AT", 1000);
  sendATCommand("AT+CMGF=1", 1000); // Set SMS to text mode
  sendATCommand("AT+CNMI=1,2,0,0,0", 1000); // SMS notification settings
  
  Serial.println("GSM Ready!");
}

void loop() {
  int sensorValues[5];
  bool sensorWet[5];

  // Read sensors and determine wet/dry
  for (int i = 0; i < 5; i++) {
    sensorValues[i] = analogRead(sensorPins[i]);
    sensorWet[i] = sensorValues[i] >= WET_THRESHOLD;
  }

  // Print sensor readings with feet
  for (int i = 0; i < 5; i++) {
    Serial.print(sensorHeights[i]);
    Serial.print("ft (A");
    Serial.print(i);
    Serial.print("): ");
    Serial.print(sensorValues[i]);
    Serial.print(" -> ");
    Serial.println(sensorWet[i] ? "WET" : "DRY");
  }

  // CHECK FOR SENSOR ERRORS (gaps in wet sensors)
  bool sensorError = checkSensorDisarray(sensorWet);
  
  if (sensorError) {
    Serial.println("⚠️ SENSOR DISARRAY DETECTED!");
  }

  // Determine highest consecutive wet sensor from bottom (only if no error)
  int waterLevelSensor = -1;
  if (!sensorError) {
    for (int i = 0; i < 5; i++) {
      if (sensorWet[i]) {
        waterLevelSensor = i;
      } else {
        break;
      }
    }
  }

  Serial.print("Detected water level: ");
  if (sensorError) {
    Serial.println("ERROR - SENSORS IN DISARRAY");
  } else if (waterLevelSensor == -1) {
    Serial.println("NONE");
  } else {
    Serial.print(sensorHeights[waterLevelSensor]);
    Serial.println(" feet");
  }

  // Handle SENSOR ERROR alerts with confirmation
  if (sensorError != stableSensorError) {
    errorStartTime = millis(); // reset timer
    stableSensorError = sensorError;
  } else {
    if (millis() - errorStartTime >= CONFIRM_DELAY) {
      if (stableSensorError && !lastSensorError) {
        // Sensor error just occurred
        sendSMSToAll_Error(sensorWet);
        lastSensorError = true;
        lastAlertLevel = -99; // Reset water level tracking
      } else if (!stableSensorError && lastSensorError) {
        // Sensors back to normal
        sendSMSToAll_ErrorResolved();
        lastSensorError = false;
      }
      errorStartTime = millis();
    }
  }

  // Handle normal water level alerts (only if no sensor error)
  if (!sensorError && !lastSensorError) {
    if (waterLevelSensor != stableWaterLevel) {
      levelStartTime = millis();
      stableWaterLevel = waterLevelSensor;
    } else {
      if (millis() - levelStartTime >= CONFIRM_DELAY) {
        if (stableWaterLevel > lastAlertLevel) {
          sendSMSToAll_Rise(stableWaterLevel);
          lastAlertLevel = stableWaterLevel;
        } else if (stableWaterLevel < lastAlertLevel) {
          sendSMSToAll_Drop(stableWaterLevel);
          lastAlertLevel = stableWaterLevel;
        }
        levelStartTime = millis();
      }
    }
  }

  Serial.println("--------------------------------");
  delay(500);
}

// Check if sensors are in disarray (dry sensor between wet sensors)
bool checkSensorDisarray(bool sensorWet[]) {
  // Look for pattern: WET - DRY - WET
  for (int i = 0; i < 3; i++) { // Check sensors for gaps
    if (sensorWet[i] && !sensorWet[i+1] && sensorWet[i+2]) {
      return true; // Found a gap!
    }
  }
  return false;
}

// Helper function to send AT command and read response
void sendATCommand(const char* cmd, int timeout) {
  Serial.print("Sending: ");
  Serial.println(cmd);
  
  SIM900A.println(cmd);
  
  long int time = millis();
  String response = "";
  
  while ((time + timeout) > millis()) {
    while (SIM900A.available()) {
      char c = SIM900A.read();
      response += c;
    }
  }
  
  Serial.print("Response: ");
  Serial.println(response);
}

// Generic function to send SMS to one number
void sendSMS(const char* phoneNumber, String message) {
  Serial.print("Sending to ");
  Serial.print(phoneNumber);
  Serial.print(": ");
  
  SIM900A.println("AT+CMGF=1");
  delay(1000);
  readGSMResponse();
  
  SIM900A.print("AT+CMGS=\"");
  SIM900A.print(phoneNumber);
  SIM900A.println("\"");
  delay(1000);
  readGSMResponse();
  
  SIM900A.print(message);
  delay(500);
  
  SIM900A.write(26); // Ctrl+Z
  delay(6000);
  readGSMResponse();
  
  Serial.println("✓ Sent");
}

// Send SMS to all numbers - Sensor Error
void sendSMSToAll_Error(bool sensorWet[]) {
  Serial.println("=== SENDING SENSOR ERROR SMS TO ALL ===");
  
  // Build message once
  String message = "WARNING! Sensors in DISARRAY. Status: ";
  for (int i = 0; i < 5; i++) {
    message += String(sensorHeights[i]) + "ft=";
    message += sensorWet[i] ? "WET" : "DRY";
    if (i < 4) message += ", ";
  }
  message += ". Check sensors immediately!";
  
  // Send to all numbers
  for (int i = 0; i < numPhones; i++) {
    sendSMS(phoneNumbers[i], message);
    delay(2000); // Small delay between messages
  }
  
  Serial.println("=== ALL ERROR SMS SENT ===");
}

// Send SMS to all numbers - Error Resolved
void sendSMSToAll_ErrorResolved() {
  Serial.println("=== SENDING RESOLVED SMS TO ALL ===");
  
  String message = "INFO: Sensors back to normal operation.";
  
  for (int i = 0; i < numPhones; i++) {
    sendSMS(phoneNumbers[i], message);
    delay(2000);
  }
  
  Serial.println("=== ALL RESOLVED SMS SENT ===");
}

// Send SMS to all numbers - Water Rising
void sendSMSToAll_Rise(int sensorIndex) {
  Serial.println("=== SENDING RISE SMS TO ALL ===");
  
  String message = "ALERT! Water level RAISED to " + 
                   String(sensorHeights[sensorIndex]) + " feet.";
  
  for (int i = 0; i < numPhones; i++) {
    sendSMS(phoneNumbers[i], message);
    delay(2000);
  }
  
  Serial.println("=== ALL RISE SMS SENT ===");
}

// Send SMS to all numbers - Water Dropping
void sendSMSToAll_Drop(int sensorIndex) {
  Serial.println("=== SENDING DROP SMS TO ALL ===");
  
  String message;
  if (sensorIndex == -1) {
    message = "ALERT! Water fully DROPPED (below 1 foot).";
  } else {
    message = "ALERT! Water level DROPPED to " + 
              String(sensorHeights[sensorIndex]) + " feet.";
  }
  
  for (int i = 0; i < numPhones; i++) {
    sendSMS(phoneNumbers[i], message);
    delay(2000);
  }
  
  Serial.println("=== ALL DROP SMS SENT ===");
}

// Read and print GSM module response
void readGSMResponse() {
  delay(200);
  while (SIM900A.available()) {
    char c = SIM900A.read();
    Serial.write(c);
  }
  Serial.println();
}