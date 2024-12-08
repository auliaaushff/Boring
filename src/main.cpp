#include <ESP8266WiFi.h>
#include <DHT.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Firebase configuration
#define FIREBASE_HOST "https://boring-2d58e-default-rtdb.firebaseio.com/"
#define FIREBASE_API_KEY "AIzaSyAxVHoQT7eyRSIC0ti4gUdTDhggPKuvlHE"
#define USER_EMAIL "raihan@gmail.com"
#define USER_PASSWORD "raihan123"

// WiFi credentials
#define SSID "Assalamualaikum fakir WiFi"
#define PASSWORD "bayarosek"

// DHT Sensor
#define DHTPIN D4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Pin configurations
#define BUZZER D8
#define KIPAS D1
#define HEATER D0
#define TRIGPIN D5
#define ECHOPIN D6

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variables
unsigned long timerStart = 0;
unsigned long timerDuration = 0;
bool timerActive = false;
float distance = 0.0;
bool status = false; // Status to indicate proximity

// Timer durations in milliseconds
const unsigned long TIMER_1_MINUTE = 60000;
const unsigned long TIMER_3_MINUTES = 180000;
const unsigned long TIMER_5_MINUTES = 300000;

// Function declarations
void readSensors();
void checkFirebaseStart();
void handleTimer();
void sendDataToFirebase(float temperature, float humidity, bool status);
void controlActuators(bool state);
float getUltrasonicDistance();
unsigned long getTimerDurationFromFirebase();

void setup() {
  Serial.begin(115200);

  // Initialize DHT sensor
  dht.begin();

  // Initialize pins
  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(KIPAS, OUTPUT);
  pinMode(HEATER, OUTPUT);

  digitalWrite(KIPAS, HIGH); 
  digitalWrite(HEATER, HIGH);

  // Connect to WiFi
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected!");

  // Initialize Firebase
  config.database_url = FIREBASE_HOST;
  config.api_key = FIREBASE_API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready. Check your configuration.");
    while (1) delay(2000);
  }
}

void loop() {
  // Read sensors and update status
  readSensors();

  // Check Firebase for the "start" signal
  checkFirebaseStart();

  // Handle timer and actuators
  handleTimer();

  delay(1000);
}

// Function to read sensors and display distance
void readSensors() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read data from DHT sensor!");
    return;
  }

  distance = getUltrasonicDistance();

  if (distance < 20) {
    status = true;
    Serial.println("Proximity detected! Status: TRUE");
  } else {
    status = false;
    Serial.println("No proximity detected. Status: FALSE");
  }

  Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%, Distance: %.2f cm\n", temperature, humidity, distance);

  sendDataToFirebase(temperature, humidity, status);
}

// Function to check Firebase for "start" signal
void checkFirebaseStart() {
  if (Firebase.RTDB.getBool(&fbdo, "/sensor/start")) {
    bool start = fbdo.boolData();
    if (start && !timerActive) {
      timerDuration = getTimerDurationFromFirebase(); // Get duration from Firebase
      if (timerDuration > 0) {
        timerStart = millis();
        timerActive = true;
        Firebase.RTDB.setBool(&fbdo, "/sensor/status", true); // Update status
        Serial.println("Timer started.");
      } else {
        Serial.println("Invalid timer duration. Timer not started.");
      }
    } else if (!start && timerActive) {
      timerStart = 0;
      timerActive = false;
      controlActuators(false);
      Firebase.RTDB.setBool(&fbdo, "/sensor/status", false); // Update status
      Serial.println("Timer stopped.");
    }
  } else {
    Serial.print("Failed to get start status: ");
    Serial.println(fbdo.errorReason());
  }
}

// Function to handle timer and actuators
void handleTimer() {
  if (timerActive) {
    unsigned long elapsedTime = millis() - timerStart;

    if (elapsedTime < timerDuration) {
      controlActuators(true);
      Serial.printf("Timer running: %lu ms\n", elapsedTime);
    } else {
      timerActive = false;
      controlActuators(false);
      Firebase.RTDB.setBool(&fbdo, "/sensor/start", false); // Reset start button
      Firebase.RTDB.setBool(&fbdo, "/sensor/status", false); // Update status
      Serial.println("Timer completed.");
    }
  }
}

// Function to send sensor data to Firebase
void sendDataToFirebase(float temperature, float humidity, bool status) {
  String path = "/sensor";

  if (!Firebase.RTDB.setFloat(&fbdo, path + "/suhu", temperature)) {
    Serial.print("Failed to send temperature: ");
    Serial.println(fbdo.errorReason());
  }

  if (!Firebase.RTDB.setFloat(&fbdo, path + "/kelembapan", humidity)) {
    Serial.print("Failed to send humidity: ");
    Serial.println(fbdo.errorReason());
  }

  if (!Firebase.RTDB.setBool(&fbdo, path + "/status", status)) {
    Serial.print("Failed to send status: ");
    Serial.println(fbdo.errorReason());
  }
}

// Function to get timer duration from Firebase
unsigned long getTimerDurationFromFirebase() {
  if (Firebase.RTDB.getInt(&fbdo, "/sensor/timer")) {
    int timerValue = fbdo.intData();
    switch (timerValue) {
      case 1: return TIMER_1_MINUTE;
      case 3: return TIMER_3_MINUTES;
      case 5: return TIMER_5_MINUTES;
      default: return 0; // Invalid duration
    }
    Serial.println(timerValue);
  } else {
    Serial.print("Failed to get timer duration: ");
    Serial.println(fbdo.errorReason());
    return 0;
  }
}

// Function to control actuators
void controlActuators(bool state) {
  digitalWrite(KIPAS, state ? LOW : HIGH);
  digitalWrite(HEATER, state ? LOW : HIGH);
  if (state) {
    tone(BUZZER, 500);
  } else {
    noTone(BUZZER);
  }
}

// Function to get distance from ultrasonic sensor
float getUltrasonicDistance() {
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);

  long duration = pulseIn(ECHOPIN, HIGH);
  return (duration * 0.0343) / 2;
}
