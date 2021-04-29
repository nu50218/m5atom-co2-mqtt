#include <ArduinoMqttClient.h>
#include <M5Atom.h>
#include <WiFi.h>

#include "Adafruit_SGP30.h"
#include "Arduino_JSON.h"

// sensor
Adafruit_SGP30 sgp;

// wifi
WiFiClient wifi_client;
const char* wifi_ssid = "ssid";
const char* wifi_password = "password";

// mqtt
MqttClient mqtt_client(wifi_client);
const char* mqtt_server = "xxx.xxx.xxx.xxx";
int mqtt_port = 1883;
const char* mqtt_topic = "/hoge/fuga/piyo";

// loop config
const long interval = 5000;
unsigned long previousMillis = 0;

// display
uint8_t DisBuff[2 + 5 * 5 * 3];

void setBuff(uint8_t Rdata, uint8_t Gdata, uint8_t Bdata) {
    DisBuff[0] = 0x05;
    DisBuff[1] = 0x05;
    for (int i = 0; i < 25; i++) {
        DisBuff[2 + i * 3 + 0] = Rdata;
        DisBuff[2 + i * 3 + 1] = Gdata;
        DisBuff[2 + i * 3 + 2] = Bdata;
    }
}

void fillLED(uint8_t Rdata, uint8_t Gdata, uint8_t Bdata) {
    setBuff(Rdata, Gdata, Bdata);
    M5.dis.displaybuff(DisBuff);
}

void setup() {
    Serial.begin(115200);

    M5.begin(true, false, true);
    delay(100);
    fillLED(0xff, 0x00, 0x00);

    if (!sgp.begin()) {
        Serial.println("Sensor not found :(");
        while (1)
            ;
    }

    Serial.print("Found SGP30 serial #");
    Serial.print(sgp.serialnumber[0], HEX);
    Serial.print(sgp.serialnumber[1], HEX);
    Serial.println(sgp.serialnumber[2], HEX);

    int retry = 0;
    WiFi.begin(wifi_ssid, wifi_password);
    while (WiFi.status() != WL_CONNECTED) {
        if (retry == 5) {
            ESP.restart();
        }
        Serial.println("connecting wifi");
        retry++;
        delay(500);
    }

    if (!mqtt_client.connect(mqtt_server, mqtt_port)) {
        Serial.print("MQTT connection failed! Error code = ");
        Serial.println(mqtt_client.connectError());
        while (1)
            ;
    }
}

void loop() {
    mqtt_client.poll();

    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        fillLED(0x00, 0x00, 0xff);

        if (!sgp.IAQmeasure()) {
            Serial.println("Measurement failed");
            fillLED(0xff, 0x00, 0x00);
            return;
        }

        if (!sgp.IAQmeasureRaw()) {
            Serial.println("Raw Measurement failed");
            fillLED(0xff, 0x00, 0x00);
            return;
        }

        // format data to json
        JSONVar obj;
        obj["rawH2"] = (sgp.rawH2);
        obj["rawEthanol"] = (sgp.rawEthanol);
        obj["TVOC"] = (sgp.TVOC);
        obj["eCO2"] = (sgp.eCO2);
        String obj_str = JSON.stringify(obj);

        // debug print
        Serial.println(obj_str);

        // send data to mqtt
        mqtt_client.beginMessage(mqtt_topic);
        mqtt_client.print(obj_str);
        if (!mqtt_client.endMessage()) {
            Serial.print("MQTT connection failed!");
            fillLED(0xff, 0x00, 0x00);
            delay(5000);
            ESP.restart();
        }

        fillLED(0x00, 0xff, 0x00);
    }
}
