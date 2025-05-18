#include <WiFi.h>
#include "DHTesp.h"
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

#define DHT_PIN 15
#define BUZZER 12
#define LDR_PIN 34               // ADC1_CH6, maps 0–4095 :contentReference[oaicite:0]{index=0} 
#define SERVO_PIN  14  



unsigned long samplingInterval = 1; //5000;   // default ts = 5 s  
unsigned long sendingInterval  =  10;    // 120000; // default tu = 120 s  
unsigned long lastSampleTime = 0;  
unsigned long lastSendTime   = 0;  
uint32_t sumLight = 0;  
uint16_t sampleCount = 0;  
float thetaOffset=30;
float gammaFactor=0.75;
float Tmed=30;
float intensity;

char tempAr[6];

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
DHTesp dhtSensor;
Servo windowServo;


bool isScheduledON = false;
unsigned long scheduledOnTime;


void setupWifi();
void updateTemperature();
void setupMqtt();
void connectToBroker();
void buzzerOn(bool on);
void setup();
void receiveCallbackMyFunction(char* topic, byte* payload, unsigned int length);
void updateTemperature();
unsigned long getTime();
void checkSchedule();


void setup() {
  Serial.begin(115200);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  pinMode(LDR_PIN, INPUT);

  setupWifi();
  setupMqtt();
  
  timeClient.begin();
  timeClient.setTimeOffset(19800); // GMT+5:30
  
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  windowServo.attach(SERVO_PIN);
  windowServo.write(thetaOffset);
}


void loop() {
  if(!mqttClient.connected()) {
    connectToBroker();
  }
  mqttClient.loop();

  updateTemperature();
  // Serial.println(tempAr);
  mqttClient.publish("HOME-TEMP-AKINDU", tempAr);

 

  unsigned long now = millis();

// 1) Sample every ts
  if (now - lastSampleTime >= samplingInterval) {
    lastSampleTime = now;
    uint16_t raw = analogRead(LDR_PIN);      // raw ∈ [0,4095] :contentReference[oaicite:2]{index=2}
    sumLight += raw;                         
    sampleCount++;
  }

  // 2) Send average every tu
  if (now - lastSendTime >= sendingInterval) {
    lastSendTime = now;
    float avg = (sampleCount>0)
      ? float(sumLight)/sampleCount/4095.0f   // normalize 0–1 :contentReference[oaicite:4]{index=4}
      : 0.0f;
    intensity=1.0f-avg;
    char buf[8];
    dtostrf(intensity, 1, 3, buf);                  // “0.xxx” :contentReference[oaicite:5]{index=5}
    mqttClient.publish("HOME-LIGHT-AKINDU", buf);

    sumLight = 0;
    sampleCount = 0;
    // compute the window angle θ
    float T     = dhtSensor.getTempAndHumidity().temperature;


    float ratio = float(samplingInterval) / float(sendingInterval);
    if (ratio <= 0) ratio = 1.0f;
    
    // Use a modified log factor to prevent zero multiplication
    float logFactor;
    if (ratio > 0.95 && ratio < 1.05) {
      // If ratio is close to 1, use a small non-zero value instead of log(ratio)
      logFactor = 0.5;  // This ensures movement even when ratio=1
    } else {
      logFactor = log(ratio);
    }
    
    float theta = thetaOffset
      + (180.0f - thetaOffset)
        * intensity
        * gammaFactor
        * logFactor  // Using our modified factor instead of log(ratio)
        * (T / Tmed);

    if (theta < 0.0f) {
      theta = 0.0f;
    } else if (theta > 180.0f) {
      theta = 180.0f;
    }
    windowServo.write(theta);

    // (optional) debug-print
    Serial.print("New θ = ");
    Serial.println(theta);
    Serial.print("New gmma = ");
    Serial.println(gammaFactor);
    Serial.print("New Tmed = ");
    Serial.println(Tmed);
    Serial.print("New offset = ");
    Serial.println(thetaOffset);
    Serial.print("Intesity =");
    Serial.println(intensity);
    Serial.print("ratio = ");
    Serial.println(ratio);
    Serial.print("T = ");
    Serial.println(T);

  }
  
  checkSchedule();
  delay(1000);
}


unsigned long getTime() {
  timeClient.update();
  return timeClient.getEpochTime();
}

void buzzerOn(bool on) {
  if (on) {
    tone(BUZZER, 256);
  } else {
    noTone(BUZZER);
  }
}

void checkSchedule() {
  unsigned long currentTime = getTime();
  if (isScheduledON && currentTime >= scheduledOnTime) {
    buzzerOn(true);
    isScheduledON = false;
    mqttClient.publish("ENTC-ADMIN-MAIN-ON-OFF-ESP", "1");
    mqttClient.publish("ENTC-ADMIN-SCH-ESP-ON", "0");
    Serial.println("Schedule ON");
  }
}

void setupMqtt() {
  mqttClient.setServer("broker.hivemq.com", 1883);
  mqttClient.setCallback(receiveCallbackMyFunction);
}


void receiveCallbackMyFunction(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  payloadCharAr[length] = '\0';
  Serial.println();

  if (strcmp(topic, "ENTC-ADMIN-MAIN-ON-OFF") == 0) {
    buzzerOn(payloadCharAr[0] == '1');
  } else if (strcmp(topic, "ENTC-ADMIN-SCH-ON") == 0) {
    if (payloadCharAr[0] == 'N') {
      isScheduledON = false;
    } else {
      isScheduledON = true;
      scheduledOnTime = atol(payloadCharAr);
    }
  }else if (strcmp(topic, "SAMPLING-INTERVAL-AKINDU") == 0) {
    samplingInterval = atol(payloadCharAr) * 1000UL;  // seconds → ms :contentReference[oaicite:8]{index=8}
    Serial.println(samplingInterval);
  }
  else if (strcmp(topic, "SENDING-INTERVAL-AKINDU") == 0) {
    sendingInterval  = atol(payloadCharAr) * 1000UL;  // seconds → ms :contentReference[oaicite:9]{index=9}
    Serial.println(sendingInterval);
  }// new Node-RED servo sliders
  else if (strcmp(topic, "SERVO-OFFSET-AKINDU")==0) {
    thetaOffset = constrain(atof(payloadCharAr), 0.0f, 120.0f);
  }
  else if (strcmp(topic, "GAMMA-AKINDU")==0) {
    gammaFactor = constrain(atof(payloadCharAr), 0.0f, 1.0f);      
  }
  else if (strcmp(topic, "TMED-AKINDU")==0) {
    Tmed = constrain(atof(payloadCharAr), 10.0f, 40.0f);      
  }
}



void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32-akindu")) {
      Serial.println("connected");
      mqttClient.subscribe("ENTC-ADMIN-MAIN-ON-OFF");
      mqttClient.subscribe("ENTC-ADMIN-SCH-ON");
      mqttClient.subscribe("SAMPLING-INTERVAL-AKINDU");
      mqttClient.subscribe("SENDING-INTERVAL-AKINDU");
      mqttClient.subscribe("SERVO-OFFSET-AKINDU");
      mqttClient.subscribe("GAMMA-AKINDU");
      mqttClient.subscribe("TMED-AKINDU");

      //Subscribe
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

