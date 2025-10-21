#include <AltSoftSerial.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <math.h>

// Emergency contact number
const String EMERGENCY_PHONE = "+9188305848xx";

// GSM Module Pins
#define RX_PIN 2
#define TX_PIN 3
SoftwareSerial sim800(RX_PIN, TX_PIN);

// GPS Module Pins
AltSoftSerial neogps;
TinyGPSPlus gps;

// Other hardware pins
#define BUZZER 12
#define BUTTON 11
#define X_PIN A1
#define Y_PIN A2
#define Z_PIN A3

// Variables
String latitude, longitude;
byte updateFlag = 0;
int xAxis = 0, yAxis = 0, zAxis = 0;
int delX = 0, delY = 0, delZ = 0;
int vibration = 2, devibrate = 75;
int magnitude = 0;
int sensitivity = 20;
boolean impactDetected = false;
unsigned long lastTime;
unsigned long impactTime;
unsigned long alertDelay = 30000; // 30 seconds

void setup() {
  Serial.begin(9600);
  sim800.begin(9600);
  neogps.begin(9600);

  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  // Initialize GSM module
  sim800.println("AT");
  delay(1000);
  sim800.println("ATE1");
  delay(1000);
  sim800.println("AT+CPIN?");
  delay(1000);
  sim800.println("AT+CMGF=1");
  delay(1000);
  sim800.println("AT+CNMI=1,1,0,0,0");
  delay(1000);

  lastTime = micros();
  xAxis = analogRead(X_PIN);
  yAxis = analogRead(Y_PIN);
  zAxis = analogRead(Z_PIN);
}

void loop() {
  // Check for impact
  if (micros() - lastTime > 1999) Impact();

  if (updateFlag > 0) {
    updateFlag = 0;
    Serial.println("Impact detected!!");
    Serial.print("Magnitude: "); Serial.println(magnitude);

    getGps();
    digitalWrite(BUZZER, HIGH);
    impactDetected = true;
    impactTime = millis();
  }

  // Send alert after delay
  if (impactDetected && millis() - impactTime >= alertDelay) {
    digitalWrite(BUZZER, LOW);
    makeCall();
    sendAlert();
    impactDetected = false;
    impactTime = 0;
  }

  // Reset with button
  if (digitalRead(BUTTON) == LOW) {
    delay(200);
    digitalWrite(BUZZER, LOW);
    impactDetected = false;
    impactTime = 0;
  }

  while (sim800.available()) {
    parseData(sim800.readString());
  }
}

void Impact() {
  lastTime = micros();
  int oldX = xAxis, oldY = yAxis, oldZ = zAxis;

  xAxis = analogRead(X_PIN);
  yAxis = analogRead(Y_PIN);
  zAxis = analogRead(Z_PIN);

  vibration--;
  if (vibration < 0) vibration = 0;
  if (vibration > 0) return;

  delX = xAxis - oldX;
  delY = yAxis - oldY;
  delZ = zAxis - oldZ;

  magnitude = sqrt(sq(delX) + sq(delY) + sq(delZ));

  if (magnitude >= sensitivity) {
    updateFlag = 1;
    vibration = devibrate;
  } else if (magnitude > 15) {
    Serial.println(magnitude);
  } else {
    magnitude = 0;
  }
}

void parseData(String buff) {
  buff.trim();
  if (buff.indexOf("+CMTI") != -1) {
    int index = buff.indexOf(",");
    String temp = "AT+CMGR=" + buff.substring(index + 1) + "\r";
    sim800.println(temp);
  } else if (buff.indexOf("+CMGR") != -1) {
    if (buff.indexOf(EMERGENCY_PHONE) > 1 && buff.indexOf("get gps") > 1) {
      getGps();
      String sms_data = "GPS Location Data\rhttp://maps.google.com/maps?q=loc:" + latitude + "," + longitude;
      sendSms(sms_data);
    }
  }
}

void getGps() {
  boolean newData = false;
  unsigned long start = millis();
  while (millis() - start < 2000) {
    while (neogps.available()) {
      if (gps.encode(neogps.read())) {
        newData = true;
        break;
      }
    }
  }

  if (newData) {
    latitude = String(gps.location.lat(), 6);
    longitude = String(gps.location.lng(), 6);
  } else {
    latitude = "";
    longitude = "";
    Serial.println("No GPS data available");
  }

  Serial.print("Latitude= "); Serial.println(latitude);
  Serial.print("Longitude= "); Serial.println(longitude);
}

void sendAlert() {
  String sms_data = "Accident Alert!!\rhttp://maps.google.com/maps?q=loc:" + latitude + "," + longitude;
  sendSms(sms_data);
}

void makeCall() {
  Serial.println("Calling...");
  sim800.println("ATD" + EMERGENCY_PHONE + ";");
  delay(20000);
  sim800.println("ATH");
}

void sendSms(String text) {
  sim800.print("AT+CMGF=1\r");
  delay(1000);
  sim800.print("AT+CMGS=\"" + EMERGENCY_PHONE + "\"\r");
  delay(1000);
  sim800.print(text);
  delay(100);
  sim800.write(0x1A); // End SMS
  delay(1000);
  Serial.println("SMS sent successfully.");
}
