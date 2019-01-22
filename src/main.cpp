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
void MQTTKeepTrack();
void MQTTpubISR();


uint8_t flag_touchR = 0;
uint8_t flag_touchF = 0;
uint8_t flag_MQTTpub = 0;
int old_status = 0;
int old_duty = 0;


Ticker touchDimm;
Ticker MQTTpub;
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

        MQTTpub.attach(2.0, MQTTpubISR);
}

void loop() {
        if (!client.connected()) {
                reconnect();
        }
        client.loop();
        dimmer();
        touchAutomat();
        if(flag_MQTTpub) {
                MQTTKeepTrack();
                flag_MQTTpub = 0;
        }


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
                        char buffer[100];
                        sprintf(buffer,"%s%s%s","/lampen/",DEVICE_NAME,"/status");
                        client.subscribe(buffer);
                        sprintf(buffer,"%s%s%s","/lampen/",DEVICE_NAME,"/pwm");
                        client.subscribe(buffer);
                        client.subscribe("/lampen/ada");

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
                touchT = 0;
                break;
        case 3:
                if(flag_touchF) {
                        if((touchT + 1000)>= millis() && touchT) {
                                touchDimm.detach();
                                flag_touchR = 0;
                                flag_touchF = 0;
                                status = 0;
                                //Serial.println("Touch losgelassen");
                        }
                        touchT = millis();
                        flag_touchF = 0;
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
                touchT = 0;
                break;
        case 6:
                if(flag_touchF) {
                        if((touchT + 1000)>= millis() && touchT) {
                                touchDimm.detach();
                                flag_touchR = 0;
                                flag_touchF = 0;
                                status = 0;
                                //Serial.println("Touch losgelassen");
                        }
                        touchT = millis();
                        flag_touchF = 0;
                }
                break;
        }


}

void MQTTKeepTrack(){
        if(client.connected()) {
                if(old_status != dimmer_status()) {
                        char buffer[100];
                        sprintf(buffer,"%s%s%s","/",DEVICE_NAME,"/status");
                        old_status = dimmer_status();
                        if(old_status == 0) {
                                client.publish(buffer, "aus");
                        }
                        if(old_status == 1) {
                                client.publish(buffer, "ein");
                        }

                }
                if(old_duty != dimmer_getDuty()) {
                        char buffer[100];
                        char payload[10];
                        old_duty = dimmer_getDuty();
                        sprintf(payload,"%i",old_duty);
                        sprintf(buffer,"%s%s%s","/",DEVICE_NAME,"/pwm");
                        client.publish(buffer, payload);

                }
        }
}


void callback(char* topic, byte* payload, unsigned int length){
        if(!strcmp("/lampen/ada",topic)) {                      //Possible commands:"NAME ein"
                Serial.println("Adafruit Input");                                 //"NAME auf %"
                char buffer[100];                                                 //"NAME aus"
                snprintf(buffer,length+1,"%s",payload);                           //ein
                String data = String(buffer);                                     //aus
                Serial.print("Payload: "); Serial.println(data);
                if(!data.compareTo("ein")) {
                        dimmer_on();
                        return;
                }
                if(!data.compareTo("aus")) {
                        dimmer_off();
                        return;
                }
                if(data.startsWith(DEVICE_NAME)) {
                        if(data.indexOf("ein") != -1) {
                                dimmer_on();
                                return;
                        }
                        if(data.indexOf("aus") != -1) {
                                dimmer_off();
                                return;
                        }
                        int index = 0;
                        if((index = data.indexOf("auf ")) != -1) {
                                char number[4];
                                int duty;
                                for(int i=0; i<=4; i++) {
                                        number[i] = buffer[index+4+i];
                                }
                                duty = atoi(number);
                                dimmer_move(duty);
                        }
                }
                return;
        }
        char buffer[100];
        sprintf(buffer,"%s%s%s","/lampen/",DEVICE_NAME,"/status");
        if(!strcmp(buffer,topic)) {
                char buffer[10];
                snprintf(buffer,length+1,"%s",payload);
                if(!strcmp(buffer,"ein")) {
                        dimmer_on();
                        return;
                }
                if(!strcmp(buffer,"aus")) {
                        dimmer_off();
                        return;
                }
        }
        sprintf(buffer,"%s%s%s","/lampen/",DEVICE_NAME,"/pwm");
        if(!strcmp(buffer,topic)) {
                int duty;
                char buffer[10];
                snprintf(buffer,length+1,"%s",payload);
                duty = atoi(buffer);
                dimmer_move(duty);
        }




}

void MQTTpubISR(){
        flag_MQTTpub = 1;
}

void touchISR(){
        if(flag_touchR) {
                flag_touchF = 1;
                flag_touchR = 0;
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
