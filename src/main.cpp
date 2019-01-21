#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include "ACDimmer.h"
#include "config.h"

#define DEVICE_NAME "DimmerTest"


void callback(char* topic, byte* payload, unsigned int length);
void reconnect();


WiFiClient espClient;
PubSubClient client(MQTT_IP,MQTT_PORT,callback,espClient);


void setup() {
        Serial.begin(115200);
        init_dimmer();
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID,WIFI_PASS);
        while (WiFi.status() != WL_CONNECTED) {
                delay(500);
                Serial.print(".");
        }
        while(!Serial);
        Serial.println("");
        Serial.print("Connected, IP address: ");
        Serial.println(WiFi.localIP());

}

void loop() {
        if (!client.connected()) {
                reconnect();
        }
        client.loop();
        dimmer();

        //if(readings>=40) {
        //Serial.print(" P: "); Serial.println(period,DEC);
        //Serial.print(" duty: "); Serial.println(duty,DEC);
        //Serial.print(" timer: "); Serial.println(tLow,DEC);
        //readings = 0;
        //}

}

void reconnect() {
        // Loop until we're reconnected
        while (!client.connected()) {
                Serial.print("Attempting MQTT connection...");
                // Create a random client ID
                String clientId = "ESP8266Client-";
                clientId += DEVICE_NAME;
                // Attempt to connect
                if (client.connect(clientId.c_str(),MQTT_USR,MQTT_PW)) {
                        Serial.print("connected as ");
                        Serial.print(clientId.c_str()), Serial.println("");
                        client.subscribe("/lampen/pwm");
                        //client.subscribe("/lampen/nachttisch");

                } else {
                        Serial.print("failed, rc=");
                        Serial.print(client.state());
                        Serial.println(" try again in 5 seconds");
                        // Wait 5 seconds before retrying
                        delay(5000);
                }
        }
}


void callback(char* topic, byte* payload, unsigned int length){
        int duty;
        char buffer[100];
        snprintf(buffer,length+1,"%s",payload);
        duty = atoi(buffer);
        dimmer_move(duty);
}
