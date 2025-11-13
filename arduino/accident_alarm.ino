// Accident Alarming System - Arduino sketch
// Hardware: Arduino UNO, ADXL-like accelerometer (analog), NEO-6M GPS, SIM800L GSM, buzzer, button
// Author: adapted from your project report. Replace EMERGENCY_PHONE and tune sensitivity as needed.

#include <AltSoftSerial.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <math.h>

// ----------------- CONFIGURATION -----------------
const String EMERGENCY_PHONE = "+916309960479"; // <-- REPLACE with actual emergency number

// GSM Serial (SIM800L) connected to Arduino digital pins (use SoftwareSerial)
#define GSM_RX_PIN 2   // Arduino RX <- SIM800L TX
#define GSM_TX_PIN 3   // Arduino TX -> SIM800L RX
SoftwareSerial sim800(GSM_RX_PIN, GSM_TX_PIN);

// GPS Serial (AltSoftSerial uses specific pins on UNO)
AltSoftSerial neoGPSSerial;
TinyGPSPlus gps;

// Pins and sensors (analog pins for accelerometer)
#define BUZZER_PIN 12
#define BUTTON_PIN 11
#define X_PIN A1
#define Y_PIN A2
#define Z_PIN A3

// Tuning parameters
int sensitivity = 20;        // impact threshold (tune based on sensor)
int vibration = 2;           // debounce counter
int devibrate = 75;          // debounce reset value
int magnitudeValue = 0;

// Internal state
byte updateFlag = 0;
boolean impactDetected = false;
unsigned long time1_us;
unsigned long impact_time_ms;
unsigned long alert_delay_ms = 30000UL; // wait 30s before sending call+sms (as in your document)

// ----------------- SETUP -----------------
void setup() {
  Serial.begin(9600);
  sim800.begin(9600);
  neoGPSSerial.begin(9600);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // initialize serial/GSM
  Serial.println("Initializing SIM800L...");
  sendAT("AT", "OK", 2000);
  sendAT("ATE1", "OK", 2000);
  sendAT("AT+CMGF=1", "OK", 2000);     // text mode
  sendAT("AT+CNMI=1,1,0,0,0", "OK", 2000); // new message indications

  // initialize accelerometer baseline reads
  time1_us = micros();
}

// ----------------- MAIN LOOP -----------------
void loop() {
  // impact sampling (every ~2 ms in the original code)
  if (micros() - time1_us > 1999UL) Impact();

  if (updateFlag > 0) {
    updateFlag = 0;
    Serial.println("Impact detected!!");
    Serial.print("Magnitude: ");
    Serial.println(magnitudeValue);
    getGps();                     // fetch GPS fix
    digitalWrite(BUZZER_PIN, HIGH);
    impactDetected = true;
    impact_time_ms = millis();
  }

  if (impactDetected) {
    if (millis() - impact_time_ms >= alert_delay_ms) {
      digitalWrite(BUZZER_PIN, LOW);
      makeCall();
      delay(1000);
      sendAlert();
      impactDetected = false;
      impact_time_ms = 0;
    }
  }

  // manual reset button: if pressed, stop alert
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    impactDetected = false;
    impact_time_ms = 0;
    Serial.println("Manual reset pressed - alert cancelled.");
  }

  // handle incoming data from GSM or USB serial passthrough to SIM800 (optional)
  while (sim800.available()) {
    String s = sim800.readString();
    parseData(s); // process incoming SMS notifications if any
  }
  while (Serial.available()) {
    sim800.println(Serial.readString());
  }
}

// ----------------- IMPACT (ACCEL) READING -----------------
void Impact() {
  time1_us = micros();
  static int oldx = 0, oldy = 0, oldz = 0;

  int xaxis = analogRead(X_PIN);
  int yaxis = analogRead(Y_PIN);
  int zaxis = analogRead(Z_PIN);

  vibration--;
  if (vibration < 0) vibration = 0;
  if (vibration > 0) {
    // still in debounce period
    oldx = xaxis; oldy = yaxis; oldz = zaxis;
    return;
  }

  int deltx = xaxis - oldx;
  int delty = yaxis - oldy;
  int deltz = zaxis - oldz;

  magnitudeValue = sqrt((long)deltx * deltx + (long)delty * delty + (long)deltz * deltz);

  if (magnitudeValue >= sensitivity) {
    updateFlag = 1;
    vibration = devibrate;
  } else {
    if (magnitudeValue > 15) {
      Serial.println(magnitudeValue);
    }
    magnitudeValue = 0;
  }

  oldx = xaxis; oldy = yaxis; oldz = zaxis;
}

// ----------------- PARSE GSM DATA (SMS handling) -----------------
void parseData(String buff) {
  Serial.println("SIM800L: " + buff);
  buff.trim();
  if (buff.length() == 0) return;

  int index = buff.indexOf("\r");
  if (index >= 0) buff.remove(0, index + 1);
  buff.trim();

  if (buff.startsWith("+CMTI")) {
    // new message indication - extract index
    int commaIndex = buff.indexOf(",");
    if (commaIndex > 0) {
      String idxStr = buff.substring(commaIndex + 1);
      idxStr.trim();
      String cmd = "AT+CMGR=" + idxStr;
      sim800.println(cmd);
    }
  } else if (buff.startsWith("+CMGR")) {
    // message content follows - check for emergency commands
    if (buff.indexOf("get gps") >= 0) {
      getGps();
      String sms_data = "GPS Location Data\r\n";
      sms_data += "http://maps.google.com/maps?q=loc:";
      // latitude and longitude variables from getGps must be global/static; here we will construct after getGps
      // For simplicity getGps() stores to global strings latStr and lonStr
      extern String latStr, lonStr;
      sms_data += latStr + "," + lonStr;
      sendSms(sms_data);
    }
  }
}

// ----------------- GPS FETCH -----------------
// store GPS coords in globals for SMS
String latStr = "";
String lonStr = "";

void getGps() {
  boolean newData = false;
  unsigned long start = millis();
  while (millis() - start < 2000) {
    while (neoGPSSerial.available()) {
      if (gps.encode(neoGPSSerial.read())) {
        newData = true;
        break;
      }
    }
  }
  if (newData && gps.location.isValid()) {
    latStr = String(gps.location.lat(), 6);
    lonStr = String(gps.location.lng(), 6);
    Serial.print("Latitude= "); Serial.println(latStr);
    Serial.print("Longitude= "); Serial.println(lonStr);
  } else {
    Serial.println("No GPS data is available");
    latStr = ""; lonStr = "";
  }
}

// ----------------- ALERTING -----------------
void sendAlert() {
  String sms_data = "Accident Alert!!\r\n";
  sms_data += "http://maps.google.com/maps?q=loc:";
  sms_data += latStr + "," + lonStr;
  sendSms(sms_data);
}

void makeCall() {
  Serial.println("Calling emergency contact...");
  sim800.println("ATD" + EMERGENCY_PHONE + ";");
  delay(20000); // keep call for 20s
  sim800.println("ATH"); // hang up
  delay(1000);
}

void sendSms(String text) {
  if (EMERGENCY_PHONE.length() == 0) {
    Serial.println("Emergency phone not set.");
    return;
  }
  sim800.print("AT+CMGF=1\r");
  delay(1000);
  sim800.print("AT+CMGS=\"" + EMERGENCY_PHONE + "\"\r");
  delay(1000);
  sim800.print(text);
  delay(100);
  sim800.write(0x1A); // CTRL+Z to send SMS
  delay(1000);
  Serial.println("SMS Sent (attempt).");
}

// ----------------- HELPER: send AT and wait for response -----------------
bool sendAT(String at_command, String expected_answer, unsigned int timeout) {
  String response = "";
  unsigned long previous = millis();

  // flush input
  while (sim800.available() > 0) sim800.read();

  sim800.println(at_command);
  do {
    while (sim800.available() > 0) {
      char c = sim800.read();
      response += c;
      if (response.indexOf(expected_answer) != -1) {
        Serial.println("AT response: " + response);
        return true;
      }
    }
  } while (millis() - previous < timeout);

  Serial.println("AT timeout, response: " + response);
  return false;
}

