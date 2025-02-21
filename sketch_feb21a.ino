#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h> // Include for SSL/TLS

// WiFi Configuration
const char* WIFI_SSID = "KD's A15";
const char* WIFI_PASSWORD = "albertbiju0304";

// Server Configuration
const char* SERVER_URL = "http://your-server-address.com"; // Replace with your server URL
const char* SOIL_DATA_ENDPOINT = "/api/soil_data"; // Endpoint to send soil data
const char* IRRIGATION_CONTROL_ENDPOINT = "/api/irrigation_control"; // Endpoint to fetch irrigation commands

// WeatherAPI Configuration
const char* WEATHERAPI_KEY = "82b6318f73494690b4b124757251102";
const char* CITY = "Pala";  // Updated city to Pala

// Supabase credentials
const char* supabaseUrl = "https://yaumextpxkxbbrdczypq.supabase.co";
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlhdW1leHRweGt4YmJyZGN6eXBxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3Mzk3MDg5ODgsImV4cCI6MjA1NTI4NDk4OH0.khiXPj6b6hG3i5zoqod9cPJKQ5bFj2OK7c7i5iMYW2I";

// Pin Configurations
#define SOIL_MOISTURE_PIN  5     // Analog pin for soil moisture sensor
#define PUMP_CONTROL_PIN   15    // Digital pin to control water pump
#define MOISTURE_POWER_PIN 2     // Power control for moisture sensor
#define MANUAL_BUTTON_PIN  4     // Digital pin for manual pump control button

// Irrigation Parameters
const int DRY_THRESHOLD = 300;    // Adjust based on your soil moisture sensor
const int WET_THRESHOLD = 700;    // Adjust based on your soil moisture sensor
const int PUMP_DURATION = 10000;  // 10 seconds pump duration
const int MANUAL_PUMP_DURATION = 5000;  // 5 seconds for manual pump activation

// Weather and Moisture Variables
float temperature = 0.0;
int humidity = 0;
float rainfall = 0.0;
int soilMoisture = 0;

// Manual pump control
bool manualPumpActive = false;
unsigned long manualPumpStartTime = 0;

// Function declarations
void connectToWiFi();
void checkManualButton();
void activatePump(int duration);
void deactivatePump();
int readSoilMoisture();
void checkIrrigationNeeds();
void publishSoilData();
void fetchWeatherData();
void fetchIrrigationCommand();

void setup() {
  Serial.begin(115200);
  
  // Pin Configurations
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(PUMP_CONTROL_PIN, OUTPUT);
  pinMode(MOISTURE_POWER_PIN, OUTPUT);
  pinMode(MANUAL_BUTTON_PIN, INPUT_PULLUP);  // Enable internal pull-up resistor
  
  // Ensure pump is off initially
  digitalWrite(PUMP_CONTROL_PIN, LOW);
  
  // Connect to WiFi
  connectToWiFi();
}

void loop() {
  // Reconnect if WiFi disconnects
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
  
  // Check manual button state with debounce
  checkManualButton();
  
  // Check if manual pump needs to be turned off
  if (manualPumpActive && (millis() - manualPumpStartTime >= MANUAL_PUMP_DURATION)) {
    deactivatePump();
    manualPumpActive = false;
  }
  
  // Read soil moisture
  soilMoisture = readSoilMoisture();
  
  // Publish soil moisture data periodically
  static unsigned long lastMoisturePublish = 0;
  if (millis() - lastMoisturePublish > 300000) { // Every 5 minutes
    publishSoilData();
    lastMoisturePublish = millis();
  }
  
  // Fetch weather data periodically
  static unsigned long lastWeatherUpdate = 0;
  if (millis() - lastWeatherUpdate > 900000) { // Every 15 minutes
    fetchWeatherData();
    lastWeatherUpdate = millis();
  }
  
  // Fetch irrigation command periodically
  static unsigned long lastCommandFetch = 0;
  if (millis() - lastCommandFetch > 60000) { // Every 1 minute
    fetchIrrigationCommand();
    lastCommandFetch = millis();
  }
  
  // Only check automatic irrigation if manual control is not active
  if (!manualPumpActive) {
    checkIrrigationNeeds();
  }
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }
  
  Serial.println("Connected to WiFi");
}

void checkManualButton() {
  static unsigned long lastDebounceTime = 0;
  static int lastButtonState = HIGH;
  int buttonState = digitalRead(MANUAL_BUTTON_PIN);
  
  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > 50) {
    if (buttonState == LOW && lastButtonState == HIGH) {
      manualPumpActive = true;
      manualPumpStartTime = millis();
      activatePump(MANUAL_PUMP_DURATION);
    }
  }
  
  lastButtonState = buttonState;
}

void activatePump(int duration) {
  digitalWrite(PUMP_CONTROL_PIN, HIGH);
  Serial.println("Pump activated");
  delay(duration);
  deactivatePump();
}

void deactivatePump() {
  digitalWrite(PUMP_CONTROL_PIN, LOW);
  Serial.println("Pump deactivated");
}

int readSoilMoisture() {
  digitalWrite(MOISTURE_POWER_PIN, HIGH);
  delay(100); // Allow the sensor to stabilize
  int moistureValue = analogRead(SOIL_MOISTURE_PIN);
  digitalWrite(MOISTURE_POWER_PIN, LOW);
  Serial.print("Soil Moisture: ");
  Serial.println(moistureValue);
  return moistureValue;
}

void checkIrrigationNeeds() {
  if (soilMoisture < DRY_THRESHOLD) {
    activatePump(PUMP_DURATION);
  } else if (soilMoisture > WET_THRESHOLD) {
    deactivatePump();
  }
}

void publishSoilData() {
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["soil_moisture"] = soilMoisture;
  jsonDoc["temperature"] = temperature;
  jsonDoc["humidity"] = humidity;
  jsonDoc["rainfall"] = rainfall;
  
  String jsonString;
  serializeJson(jsonDoc, jsonString);
  
  HTTPClient http;
  http.begin(String(SERVER_URL) + String(SOIL_DATA_ENDPOINT));
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.POST(jsonString);
  if (httpResponseCode == HTTP_CODE_OK) {
    Serial.println("Soil data published");
  } else {
    Serial.println("Failed to publish soil data");
  }
  
  http.end();
}

void fetchWeatherData() {
  String url = "http://api.weatherapi.com/v1/current.json?key=" + String(WEATHERAPI_KEY) + "&q=" + String(CITY);
  
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument jsonDoc(1024);
    deserializeJson(jsonDoc, payload);
    
    temperature = jsonDoc["current"]["temp_c"];
    humidity = jsonDoc["current"]["humidity"];
    rainfall = jsonDoc["current"]["precip_mm"];
    
    Serial.println("Weather data fetched");
  } else {
    Serial.println("Failed to fetch weather data");
  }
  
  http.end();
}

void fetchIrrigationCommand() {
  HTTPClient http;
  http.begin(String(SERVER_URL) + String(IRRIGATION_CONTROL_ENDPOINT));
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument jsonDoc(200);
    deserializeJson(jsonDoc, payload);
    
    bool shouldActivatePump = jsonDoc["activate_pump"];
    if (shouldActivatePump) {
      activatePump(PUMP_DURATION);
    } else {
      deactivatePump();
    }
    
    Serial.println("Irrigation command fetched");
  } else {
    Serial.println("Failed to fetch irrigation command");
  }
  
  http.end();
}