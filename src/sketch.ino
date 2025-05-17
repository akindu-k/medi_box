#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <DHTesp.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define DHT_PIN 15
#define BUZZER 12

bool isScheduledON = false;
unsigned long scheduledOnTime;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

char tempAr[6];
DHTesp dhtSensor;


void setup(){
    Serial.begin(115200);
    setupWifi();
    setupMqtt();
    dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

    timeClient.begin();
    timeClient.setTimeOffset(5.5*3600);

    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
}


void loop(){
if (!mqttClient.connected()){
connectToBroker();
 }
    mqttClient.loop();
    updateTemperature();
    Serial.println(tempAr);
    mqttClient.publish("EE-ADMIN-TEMP", tempAr);

    checkSchedule();

    delay(1000);
}



void buzzerOn(bool state){
    if (state){
        tone(BUZZER, 256);
    } else {
        noTone(BUZZER);
    }
}


void setupMqtt(){
    mqttClient.setServer("broker.hivemq.com", 1883);
    mqttClient.setCallback(receiveCallback);
}


void receiveCallback(char* topic, byte* payload, unsigned int length){
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    
    char payloadCharAr[length];

    for (int i = 0; i < length; i++){
        Serial.print((char)payload[i]);
        payloadCharAr[i] = (char)payload[i];
    }

    Serial.println();

    if(strcmp(topic, "EE-ADMIN-BUZZER-ON-OFF") == 0){

        buzzerOn(payloadCharAr[0] == '1');
    } else if (strcmp(topic, "EE-ADMIN-SCH-ON") == 0){
        if (payloadCharAr[0] == 'N'){
            isScheduledON = false;
            
        } else {
            isScheduledON = true;
            scheduledOnTime = atol(payloadCharAr);
            
        }

    }   

}


void connectToBroker(){
    while (!mqttClient.connected()){
    Serial.print("Connecting to MQTT broker...");
    if (mqttClient.connect("ESP32Client")){
        Serial.println("connected");
        mqttClient.subscribe("EE-ADMIN-BUZZER-ON-OFF");
        mqttClient.subscribe("EE-ADMIN-SCH-ON");
    } else {
        Serial.print("failed, rc=");
        Serial.print(mqttClient.state());
        delay(5000);
        }
    }
}


void updateTemperature(){
TempAndHumidity data = dhtSensor.getTempAndHumidity();
String(data.temperature,2).toCharArray(tempAr, 6);
}


void setupWifi(){
Serial.println();
Serial.print("Connecting to");
Serial.println("Wokwi-GUEST");
WiFi.begin("Wokwi-GUEST", "");
while (WiFi.status() != WL_CONNECTED) {
delay(500);
Serial.print(".");
 }
Serial.println("WiFi connected");
Serial.print("IP address: ");
Serial.println(WiFi.localIP());
}


unsigned long getTime(){
    timeClient.update();
    return timeClient.getEpochTime();
}


void checkSchedule(){

    if (isScheduledON){
        unsigned long currentTime = getTime();
        if (currentTime > scheduledOnTime){
            buzzerOn(true);
            isScheduledON = false;
            Serial.println("Scheduled ON");
        }
            
    }
        
}


