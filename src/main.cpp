#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include "ACDimmer.h"
#include "config.h"


#define TOUCH D7
#define TOUCHTIME 1000

void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void touchISR();
void touchDTISR_up();
void touchDTISR_down();
void touchAutomat();


uint8_t flag_touchR = 0;
uint8_t flag_touchF = 0;


Ticker touchDimm;
WiFiClient espClient;
PubSubClient client(MQTT_IP,MQTT_PORT,callback,espClient);


void setup() {
        Serial.begin(115200);
        init_dimmer();
        pinMode(TOUCH,INPUT);
        attachInterrupt(digitalPinToInterrupt(TOUCH), touchISR, CHANGE);
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
        touchAutomat();



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

void touchAutomat(){
        static unsigned long touchT = 0;
        static unsigned long betweenT = 0;
        static int status = 0;

        switch (status) {
        case 0:
                if(flag_touchR) {
                        touchT = millis();
                        status = 1;
                }
                break;
        case 1:
                if((touchT+TOUCHTIME)<=millis()) {
                        status = 2;

                }

                if(flag_touchF) {
                        if((touchT+TOUCHTIME)>=millis()) {
                                status = 4;
                                betweenT = millis();
                                flag_touchR = 0;
                                flag_touchF = 0;

                        }
                        break;
                }
                break;
        case 2:
                touchDimm.attach_ms(50, touchDTISR_up);
                status = 3;
                break;
        case 3:
                if(flag_touchF) {
                        touchDimm.detach();
                        flag_touchR = 0;
                        flag_touchF = 0;
                        status = 0;
                }
                break;
        case 4:
                if((betweenT + 500)<=millis()) {
                        if(dimmer_status()) {
                                dimmer_off();
                        }else{
                                dimmer_on();
                        }
                        status = 0;
                        break;
                }
                if(flag_touchR) {
                        status = 5;
                }


                break;
        case 5:
                touchDimm.attach_ms(50, touchDTISR_down);
                status = 6;
                break;
        case 6:
                if(flag_touchF) {
                        touchDimm.detach();
                        flag_touchR = 0;
                        flag_touchF = 0;
                        status = 0;
                }
                break;
        }


}


void callback(char* topic, byte* payload, unsigned int length){
        int duty;
        char buffer[100];
        snprintf(buffer,length+1,"%s",payload);
        duty = atoi(buffer);
        dimmer_move(duty);
}

void touchISR(){
        if(flag_touchR) {
                flag_touchF = 1;
        }else{
                flag_touchR = 1;
                flag_touchF = 0;
        }

}

void touchDTISR_down(){
        dimmer_down();
}
void touchDTISR_up(){
        dimmer_up();
}
