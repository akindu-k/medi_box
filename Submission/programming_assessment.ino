#include <WiFi.h>
#include "DHTesp.h"
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

// Pin Definitions
#define DHT_PIN     15
#define LDR_PIN     34  
#define SERVO_PIN   14

// Network Settings
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";
const char* MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "ESP32-akindu";

// MQTT Topics
const char* TOPIC_PUBLISH_TEMP = "HOME-TEMP-AKINDU";
const char* TOPIC_PUBLISH_LIGHT = "HOME-LIGHT-AKINDU";
const char* TOPIC_PUBLISH_SCHEDULED_ON = "ENTC-ADMIN-SCH-ESP-ON";
const char* TOPIC_PUBLISH_MAIN_ON_OFF = "ENTC-ADMIN-MAIN-ON-OFF-ESP";

const char* TOPIC_SUBSCRIBE_MAIN_ON_OFF = "ENTC-ADMIN-MAIN-ON-OFF";
const char* TOPIC_SUBSCRIBE_SCHEDULED_ON = "ENTC-ADMIN-SCH-ON";
const char* TOPIC_SUBSCRIBE_SAMPLING_INTERVAL = "SAMPLING-INTERVAL-AKINDU";
const char* TOPIC_SUBSCRIBE_SENDING_INTERVAL = "SENDING-INTERVAL-AKINDU";
const char* TOPIC_SUBSCRIBE_SERVO_OFFSET = "SERVO-OFFSET-AKINDU";
const char* TOPIC_SUBSCRIBE_GAMMA = "GAMMA-AKINDU";
const char* TOPIC_SUBSCRIBE_TMED = "TMED-AKINDU";

// Time Settings
const int GMT_OFFSET_SECONDS = 19800;  // GMT+5:30

// Timing variables
unsigned long samplingInterval = 1;    // Default sampling interval in milliseconds
unsigned long sendingInterval = 10;    // Default sending interval in milliseconds
unsigned long lastSampleTime = 0;
unsigned long lastSendTime = 0;

// Data collection variables
uint32_t sumLight = 0;
uint16_t sampleCount = 0;

// Servo control parameters
float thetaOffset = 30;
float gammaFactor = 0.75;
float Tmed = 30;
float intensity;

// Temperature reading
char tempAr[6];

// Schedule variables
bool isScheduledON = false;
unsigned long scheduledOnTime;

// Objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
DHTesp dhtSensor;
Servo windowServo;

// Function prototypes
void setupWifi();
void setupSensors();
void setupMqtt();
void connectToBroker();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void updateTemperature();
void handleLightSampling();
void handleServoControl();
unsigned long getTime();
void checkSchedule();

/**
 * Setup function - initializes all components
 */
void setup() {
  Serial.begin(115200);
  
  // Initialize pins
 

  pinMode(LDR_PIN, INPUT);
  
  // Setup components
  setupWifi();
  setupSensors();
  setupMqtt();
  
  // Initialize time client
  timeClient.begin();
  timeClient.setTimeOffset(GMT_OFFSET_SECONDS);
}

/**
 * Main program loop
 */
void loop() {
  // Check MQTT connection
  if (!mqttClient.connected()) {
    connectToBroker();
  }
  mqttClient.loop();
  
  // Update and publish temperature
  updateTemperature();
  mqttClient.publish(TOPIC_PUBLISH_TEMP, tempAr);
  
  // Handle light sampling and servo control
  handleLightSampling();
  
  // Check scheduled events
  checkSchedule();
  
  delay(1000);
}

/**
 * Sets up WiFi connection
 */
void setupWifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 * Sets up DHT and servo sensors
 */
void setupSensors() {
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  windowServo.attach(SERVO_PIN);
  windowServo.write(thetaOffset);
}

/**
 * Sets up MQTT client and callback
 */
void setupMqtt() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

/**
 * Connects to MQTT broker and subscribes to topics
 */
void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.println("connected");
      
      // Subscribe to topics
      mqttClient.subscribe(TOPIC_SUBSCRIBE_MAIN_ON_OFF);
      mqttClient.subscribe(TOPIC_SUBSCRIBE_SCHEDULED_ON);
      mqttClient.subscribe(TOPIC_SUBSCRIBE_SAMPLING_INTERVAL);
      mqttClient.subscribe(TOPIC_SUBSCRIBE_SENDING_INTERVAL);
      mqttClient.subscribe(TOPIC_SUBSCRIBE_SERVO_OFFSET);
      mqttClient.subscribe(TOPIC_SUBSCRIBE_GAMMA);
      mqttClient.subscribe(TOPIC_SUBSCRIBE_TMED);
    } else {
      Serial.print("failed rc=");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

/**
 * Handles incoming MQTT messages
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  // Convert payload to string
  char payloadStr[length + 1];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadStr[i] = (char)payload[i];
  }
  payloadStr[length] = '\0';
  Serial.println();

  // Process messages based on topic

  if (strcmp(topic, TOPIC_SUBSCRIBE_SAMPLING_INTERVAL) == 0) {
    samplingInterval = atol(payloadStr) * 1000UL;  // seconds → ms
    Serial.println(samplingInterval);
  }
  else if (strcmp(topic, TOPIC_SUBSCRIBE_SENDING_INTERVAL) == 0) {
    sendingInterval = atol(payloadStr) * 1000UL;  // seconds → ms
    Serial.println(sendingInterval);
  }
  else if (strcmp(topic, TOPIC_SUBSCRIBE_SERVO_OFFSET) == 0) {
    thetaOffset = constrain(atof(payloadStr), 0.0f, 120.0f);
  }
  else if (strcmp(topic, TOPIC_SUBSCRIBE_GAMMA) == 0) {
    gammaFactor = constrain(atof(payloadStr), 0.0f, 1.0f);      
  }
  else if (strcmp(topic, TOPIC_SUBSCRIBE_TMED) == 0) {
    Tmed = constrain(atof(payloadStr), 10.0f, 40.0f);      
  }
}

/**
 * Updates temperature reading from DHT sensor
 */
void updateTemperature() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr, 6);
}

/**
 * Handles light sampling and servo position calculation
 */
void handleLightSampling() {
  unsigned long now = millis();

  // 1) Sample light sensor every samplingInterval
  if (now - lastSampleTime >= samplingInterval) {
    lastSampleTime = now;
    uint16_t raw = analogRead(LDR_PIN);
    sumLight += raw;
    sampleCount++;
  }

  // 2) Process and send data every sendingInterval
  if (now - lastSendTime >= sendingInterval) {
    lastSendTime = now;
    
    // Calculate average light reading
    float avg = (sampleCount > 0) ? float(sumLight) / sampleCount / 4095.0f : 0.0f;
    intensity = 1.0f - avg;
    
    // Publish light intensity
    char lightBuffer[8];
    dtostrf(intensity, 1, 3, lightBuffer);
    mqttClient.publish(TOPIC_PUBLISH_LIGHT, lightBuffer);
    
    // Reset counters
    sumLight = 0;
    sampleCount = 0;
    
    // Update servo position
    handleServoControl();
  }
}

/**
 * Calculates and sets servo position based on temperature and light
 */
void handleServoControl() {
  float currentTemp = dhtSensor.getTempAndHumidity().temperature;
  float ratio = float(samplingInterval) / float(sendingInterval);
  
  if (ratio <= 0) ratio = 1.0f;
  
  // Calculate log factor - avoid zero multiplication issue
  float logFactor;
  if (ratio > 0.95 && ratio < 1.05) {
    logFactor = 0.5;  // Ensure movement even when ratio is close to 1
  } else {
    logFactor = log(ratio);
  }
  
  // Calculate servo angle
  float theta = thetaOffset
    + (180.0f - thetaOffset)
      * intensity
      * gammaFactor
      * logFactor
      * (currentTemp / Tmed);
  
  // Constrain angle to valid range
  theta = constrain(theta, 0.0f, 180.0f);
  
  // Set servo position
  windowServo.write(theta);
  
}


/**
 * Returns current time from NTP server
 */
unsigned long getTime() {
  timeClient.update();
  return timeClient.getEpochTime();
}

/**
 * Checks if a scheduled event should be triggered
 */
void checkSchedule() {
  unsigned long currentTime = getTime();
  if (isScheduledON && currentTime >= scheduledOnTime) {
    isScheduledON = false;
    mqttClient.publish(TOPIC_PUBLISH_MAIN_ON_OFF, "1");
    mqttClient.publish(TOPIC_PUBLISH_SCHEDULED_ON, "0");
    Serial.println("Schedule ON");
  }
}