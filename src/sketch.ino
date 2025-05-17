#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <DHTesp.h>
#include <PubSubClient.h>

#define DHT_PIN 12

WiFiClient espClient;
PubSubClient mqttClient(espClient);


char tempAr[6];

DHTesp dhtSensor;



void setup(){
    Serial.begin(115200);
    setupWifi();

    setupMqtt();

    dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

}

void loop(){

    if (!mqttClient.connected()){
        connectToBroker();
    }

    mqttClient.loop();

    updateTemperature();
    Serial.println(tempAr);
    mqttClient.publish("EE-ADMIN-TEMP", tempAr);
    delay(1000);

}

void setupMqtt(){

    mqttClient.setServer("broker.hivemq.com", 1883);


}

void connectToBroker(){

    while (!mqttClient.connected()){
        Serial.print("Connecting to MQTT broker...");
        if (mqttClient.connect("ESP32Client")){
            Serial.println("connected");
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




