#include <WiFi.h>
#include "DHTesp.h"
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

// Pin Definitions
#define DHT_PIN     15
#define BUZZER_PIN  12
#define LDR_PIN     34  // ADC1_CH6, maps 0-4095
#define SERVO_PIN   14

// Default intervals
#define DEFAULT_SAMPLING_INTERVAL 5000   // default ts = 5s
#define DEFAULT_SENDING_INTERVAL  120000 // default tu = 120s

// Window control parameters
struct WindowParams {
  float thetaOffset = 30.0f;
  float gammaFactor = 0.75f;
  float Tmed = 30.0f;
  float intensity = 0.0f;
};

// Timing variables
struct TimingData {
  unsigned long samplingInterval = DEFAULT_SAMPLING_INTERVAL;
  unsigned long sendingInterval = DEFAULT_SENDING_INTERVAL;
  unsigned long lastSampleTime = 0;
  unsigned long lastSendTime = 0;
  uint32_t sumLight = 0;
  uint16_t sampleCount = 0;
};

// Schedule data
struct ScheduleData {
  bool isScheduledON = false;
  unsigned long scheduledOnTime = 0;
};

// Global objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
DHTesp dhtSensor;
Servo windowServo;

// Global variables
char tempAr[6];
WindowParams windowParams;
TimingData timingData;
ScheduleData scheduleData;

// Function declarations
void setupWifi();
void setupMqtt();
void connectToBroker();
void setupSensors();
void receiveCallbackFunction(char* topic, byte* payload, unsigned int length);
void updateTemperature();
void updateLightSample();
void updateWindowPosition();
void processLightData();
unsigned long getTime();
void checkSchedule();
void buzzerControl(bool on);

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(LDR_PIN, INPUT);
  
  setupWifi();
  setupMqtt();
  setupSensors();
}

void loop() {
  // Ensure MQTT connection
  if (!mqttClient.connected()) {
    connectToBroker();
  }
  mqttClient.loop();
  
  // Temperature update and publish
  updateTemperature();
  mqttClient.publish("ENTC-ADMIN-TEMP", tempAr);
  
  // Light sampling routine
  updateLightSample();
  
  // Process and send average light data
  processLightData();
  
  // Check scheduled actions
  checkSchedule();
  
  delay(1000);
}

void setupSensors() {
  // Initialize NTP client
  timeClient.begin();
  timeClient.setTimeOffset(19800); // GMT+5:30
  
  // Initialize DHT sensor
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  
  // Initialize servo
  windowServo.attach(SERVO_PIN);
  windowServo.write(windowParams.thetaOffset);
}

void updateLightSample() {
  unsigned long now = millis();
  
  // Sample light level every ts
  if (now - timingData.lastSampleTime >= timingData.samplingInterval) {
    timingData.lastSampleTime = now;
    uint16_t raw = analogRead(LDR_PIN); // raw ∈ [0,4095]
    timingData.sumLight += raw;
    timingData.sampleCount++;
  }
}

void processLightData() {
  unsigned long now = millis();
  
  // Send average light data every tu
  if (now - timingData.lastSendTime >= timingData.sendingInterval) {
    timingData.lastSendTime = now;
    
    // Calculate average light intensity
    float avg = (timingData.sampleCount > 0)
      ? float(timingData.sumLight) / timingData.sampleCount / 4095.0f // normalize 0-1
      : 0.0f;
    
    windowParams.intensity = 1.0f - avg;
    
    // Format and publish light data
    char buf[8];
    dtostrf(windowParams.intensity, 1, 3, buf); // "0.xxx"
    mqttClient.publish("light/data", buf);
    
    // Reset counters
    timingData.sumLight = 0;
    timingData.sampleCount = 0;
    
    // Update window position
    updateWindowPosition();
  }
}

void updateWindowPosition() {
  // Get current temperature
  float T = dhtSensor.getTempAndHumidity().temperature;
  
  // Calculate sampling-to-sending ratio
  float ratio = float(timingData.samplingInterval) / float(timingData.sendingInterval);
  if (ratio <= 0) ratio = 1.0f;
  
  // Calculate window angle θ
  float theta = windowParams.thetaOffset
    + (180.0f - windowParams.thetaOffset)
      * windowParams.intensity
      * windowParams.gammaFactor
      * log(ratio)
      * (T / windowParams.Tmed);
  
  // Set servo position
  windowServo.write(theta);
  
  // Debug output
  Serial.print("New θ = ");
  Serial.println(theta);
  Serial.print("New gmma = ");
  Serial.println(windowParams.gammaFactor);
  Serial.print("New Tmed = ");
  Serial.println(windowParams.Tmed);
  Serial.print("New offset = ");
  Serial.println(windowParams.thetaOffset);
  Serial.print("Intesity = ");
  Serial.println(windowParams.intensity);
  Serial.print("ratio = ");
  Serial.println(ratio);
  Serial.print("T = ");
  Serial.println(T);
}

unsigned long getTime() {
  timeClient.update();
  return timeClient.getEpochTime();
}

void buzzerControl(bool on) {
  if (on) {
    tone(BUZZER_PIN, 256);
  } else {
    noTone(BUZZER_PIN);
  }
}

void checkSchedule() {
  unsigned long currentTime = getTime();
  if (scheduleData.isScheduledON && currentTime >= scheduleData.scheduledOnTime) {
    buzzerControl(true);
    scheduleData.isScheduledON = false;
    mqttClient.publish("ENTC-ADMIN-MAIN-ON-OFF-ESP", "1");
    mqttClient.publish("ENTC-ADMIN-SCH-ESP-ON", "0");
    Serial.println("Schedule ON");
  }
}

void setupMqtt() {
  mqttClient.setServer("broker.hivemq.com", 1883);
  mqttClient.setCallback(receiveCallbackFunction);
}

void receiveCallbackFunction(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  // Convert payload to char array
  char payloadCharAr[length + 1];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  payloadCharAr[length] = '\0';
  Serial.println();
  
  // Process messages based on topic
  if (strcmp(topic, "ENTC-ADMIN-MAIN-ON-OFF") == 0) {
    buzzerControl(payloadCharAr[0] == '1');
  } 
  else if (strcmp(topic, "ENTC-ADMIN-SCH-ON") == 0) {
    if (payloadCharAr[0] == 'N') {
      scheduleData.isScheduledON = false;
    } else {
      scheduleData.isScheduledON = true;
      scheduleData.scheduledOnTime = atol(payloadCharAr);
    }
  }
  else if (strcmp(topic, "light/ts") == 0) {
    timingData.samplingInterval = atol(payloadCharAr) * 1000UL;  // seconds → ms
    Serial.println(timingData.samplingInterval);
  }
  else if (strcmp(topic, "light/tu") == 0) {
    timingData.sendingInterval = atol(payloadCharAr) * 1000UL;  // seconds → ms
    Serial.println(timingData.sendingInterval);
  }
  else if (strcmp(topic, "servo/offset") == 0) {
    windowParams.thetaOffset = constrain(atof(payloadCharAr), 0.0f, 120.0f);
  }
  else if (strcmp(topic, "servo/gamma") == 0) {
    windowParams.gammaFactor = constrain(atof(payloadCharAr), 0.0f, 1.0f);      
  }
  else if (strcmp(topic, "servo/Tmed") == 0) {
    windowParams.Tmed = constrain(atof(payloadCharAr), 10.0f, 40.0f);      
  }
}

void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32-12345645454")) {
      Serial.println("connected");
      
      // Subscribe to topics
      mqttClient.subscribe("ENTC-ADMIN-MAIN-ON-OFF");
      mqttClient.subscribe("ENTC-ADMIN-SCH-ON");
      mqttClient.subscribe("light/ts");
      mqttClient.subscribe("light/tu");
      mqttClient.subscribe("servo/offset");
      mqttClient.subscribe("servo/gamma");
      mqttClient.subscribe("servo/Tmed");
    } else {
      Serial.print("failed rc=");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void updateTemperature() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr, 6);
}

void setupWifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println("Wokwi-GUEST");
  
  WiFi.begin("Wokwi-GUEST", "");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}