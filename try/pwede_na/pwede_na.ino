#include <SoftwareSerial.h>

// GSM module pins
SoftwareSerial SIM900A(10, 11); // RX, TX
const char phoneNumber[] = "+639953632031"; // Replace with your number

// Water sensor pins
const int sensorPins[5] = {A0, A1, A2, A3, A4};
const int WET_THRESHOLD = 630;

// Track last water level for alerts
int lastAlertLevel = -1;
int stableWaterLevel = -1;
unsigned long levelStartTime = 0;
const unsigned long CONFIRM_DELAY = 5000;

void setup() {
  Serial.begin(9600);
  SIM900A.begin(9600);

  Serial.println("--- SYSTEM ONLINE: REALISTIC WATER LEVEL GSM ALERT ---");
  
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

  // Determine highest consecutive wet sensor from bottom
  int waterLevelSensor = -1;
  for (int i = 0; i < 5; i++) {
    if (sensorWet[i]) {
      waterLevelSensor = i;
    } else {
      break;
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

  Serial.print("Detected water level: ");
  if (waterLevelSensor == -1) Serial.println("NONE");
  else {
    Serial.print("A");
    Serial.println(waterLevelSensor);
  }

  // Handle GSM alert with 5-sec confirmation
  if (waterLevelSensor != stableWaterLevel) {
    levelStartTime = millis();
    stableWaterLevel = waterLevelSensor;
  } else {
    if (millis() - levelStartTime >= CONFIRM_DELAY) {
      if (stableWaterLevel > lastAlertLevel) {
        sendSMSRise(stableWaterLevel);
        lastAlertLevel = stableWaterLevel;
      } else if (stableWaterLevel < lastAlertLevel) {
        sendSMSDrop(stableWaterLevel);
        lastAlertLevel = stableWaterLevel;
      }
      levelStartTime = millis();
    }
  }

  Serial.println("--------------------------------");
  delay(500);
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

// Send SMS for water rising
void sendSMSRise(int sensor) {
  Serial.println("=== SENDING RISE SMS ===");
  
  // Set text mode
  SIM900A.println("AT+CMGF=1");
  delay(1000);
  readGSMResponse();
  
  // Set phone number
  SIM900A.print("AT+CMGS=\"");
  SIM900A.print(phoneNumber);
  SIM900A.println("\"");
  delay(1000);
  readGSMResponse();
  
  // Message content
  SIM900A.print("ALERT! Water level RAISED to sensor A");
  SIM900A.print(sensor);
  SIM900A.println(".");
  delay(500);
  
  // Send Ctrl+Z
  SIM900A.write(26);
  delay(6000); // Wait for send confirmation
  readGSMResponse();
  
  Serial.println("=== SMS SEND COMPLETE ===");
}

// Send SMS for water dropping
void sendSMSDrop(int sensor) {
  Serial.println("=== SENDING DROP SMS ===");
  
  SIM900A.println("AT+CMGF=1");
  delay(1000);
  readGSMResponse();
  
  SIM900A.print("AT+CMGS=\"");
  SIM900A.print(phoneNumber);
  SIM900A.println("\"");
  delay(1000);
  readGSMResponse();
  
  if (sensor == -1) {
    SIM900A.println("ALERT! Water fully DROPPED.");
  } else {
    SIM900A.print("ALERT! Water level DROPPED to sensor A");
    SIM900A.print(sensor);
    SIM900A.println(".");
  }
  delay(500);
  
  SIM900A.write(26);
  delay(6000);
  readGSMResponse();
  
  Serial.println("=== SMS SEND COMPLETE ===");
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