
/*
   Device is connected to MQTT Server.
   MQTT Topic structure I used is:
   /lampen/DEVICE_NAME/
                   /status    Gets turn on/off commands from here (from other clients)
                   /pwm       Gets the level of brightness from here 0-100

   /lampen/ada       Bridged AdafruitIO feed here. Commands come from Google Assistant via IFTTT

   /DEVICE_NAME      In these topics every device publishes its status/brightness/sensors/whatever

   Commands look like this:
   IFTT_ACTIVATION_STRING KEYWORD                        This one turns on/off all lamps that subscibed to /ada/lampen
   KEYWORD out of On[] or Off[]

   IFTT_ACTIVATION_STRING DEVICE_NAME KEYWORD            This turns on only the device with the name DEVICE_NAME.
   KEYWORD out of On[] or Off[]                          KEYWORD must be the first and last word after DEVICE_NAME

   IFTT_ACTIVATION_STRING DEVICE_NAME KEYWORD NUMBER     Dimming the lamp to NUMBER percent. everything after NUMBER will be ignored
   KEYWORD out of perc[]     NUMBER 0-100

   if there is the same word in On[]/Off[] and perc[] the last command wont work if it is said as first word after DEVICE_NAME
   and as last word of the command.
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include "ACDimmer.h"
#include "touchAutomat.h"
#include "config.h"


#define TOUCH D7

void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void MQTTKeepTrack();
void MQTTpubISR();
void httpServer_ini();
void finishedStartUp(int speed);

//---------Voice Commands Key Words--------
const String On[]= {"ein","an","auf"};
const uint8_t numOn = 3;
const String Off[] = {"aus","ab"};
const uint8_t numOff = 2;
const String perc[] = {"auf","zu"};
const uint8_t numPerc = 2;

const char* host = "esp8266-";
const char* update_path = "/firmware";
const char* update_username = USERNAME;
const char* update_password = PASSWORD;

uint8_t flag_MQTTpub = 0;
int old_status = 0;
int old_duty = 0;

ESP8266HTTPUpdateServer httpUpdater;
ESP8266WebServer httpServer(80);
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

        httpServer_ini();

        MQTTpub.attach(2.0, MQTTpubISR);

}

void loop() {
        if (!client.connected()) {
                reconnect();
        }
        finishedStartUp(800);
        client.loop();
        httpServer.handleClient();
        dimmer();
        touchAutomat();
        if(flag_MQTTpub) {
                MQTTKeepTrack();
                flag_MQTTpub = 0;
        }
        MDNS.update();


}

void finishedStartUp(int speed){
        static int state = 0;
        static int count = 0;
        static int stop = 0;
        static unsigned long time = 0;
        if(!stop) {
                switch (state) {
                case 0:
                        dimmer_move(50, speed);
                        time = millis();
                        state++;
                        break;
                case 1:
                        if((time + speed)<=millis()) state++;
                        break;
                case 2:
                        dimmer_move(20, speed);
                        time = millis();
                        state++;
                        break;
                case 3:
                        if((time + speed)<=millis()) {
                                state = 0;
                                if(count) {
                                        stop++;
                                        state = -1;
                                        dimmer_off();
                                }
                                count++;
                        }
                        break;
                }
        }


}

void httpServer_ini(){
        char buffer[100];
        sprintf(buffer,"%s%s",host,DEVICE_NAME);
        MDNS.begin(buffer);
        httpUpdater.setup(&httpServer, update_path, update_username, update_password);
        httpServer.begin();
        MDNS.addService("http", "tcp", 80);
        Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your browser and login with username '%s' and password '%s'\n", buffer, update_path, update_username, update_password);
        //------
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



void MQTTKeepTrack(){
        if(client.connected()) {
                if(old_status != dimmer_status()||old_duty != dimmer_getDuty()) {
                        const char* dd = ":"; const char* koma = ",";
                        const char* ko = "{"; const char* kc = "}";
                        const char* Smil = "\"millis\"";
                        const char* Sperc = "\"percentage\"";
                        const char* Sstatus = "\"status\"";
                        long milli = millis();
                        old_status = dimmer_status();
                        old_duty = dimmer_getDuty();
                        String millis = String(milli,DEC);
                        String status = String(old_status,DEC);
                        String percent = String(old_duty,DEC);
                        String jsonString = String(ko)+Smil+dd+millis+koma+Sstatus+dd+status+koma+Sperc+dd+percent+kc;
                        String topic = "/" + String(DEVICE_NAME);
                        char* buffer = (char*) malloc(topic.length()+1);
                        topic.toCharArray(buffer, topic.length()+1);
                        uint8_t* buffer2 = (uint8_t*) malloc(jsonString.length()+1);
                        jsonString.getBytes(buffer2, jsonString.length()+1);
                        client.publish(buffer, buffer2, jsonString.length()+1);

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
                uint8_t i = 0;
                while(i<numOn) {
                        if(!data.compareTo(On[i])) {
                                dimmer_on();
                                return;
                        }
                        i++;
                }
                i=0;
                while(i<numOff) {
                        if(!data.compareTo(Off[i])) {
                                dimmer_off();
                                return;
                        }
                        i++;
                }

                if(data.startsWith(DEVICE_NAME)) {
                        data.remove(0,strlen(DEVICE_NAME));
                        data.trim();
                        uint8_t i = 0;
                        while(i<numOn) {
                                if(data.endsWith(On[i])&&data.startsWith(On[i])) {
                                        dimmer_on();
                                        return;
                                }
                                i++;
                        }
                        i=0;
                        while(i<numOff) {
                                if(data.endsWith(Off[i])&&data.startsWith(Off[i])) {
                                        dimmer_off();
                                        return;
                                }
                                i++;
                        }
                        i=0;
                        while(i<numPerc) {
                                if(data.startsWith(perc[i])) {
                                        data.remove(0,perc[i].length());
                                        int duty;
                                        duty = data.toInt();
                                        dimmer_move(duty);
                                }
                                i++;
                        }

                }
                return;
        }
        char buffer[100];
        sprintf(buffer,"%s%s%s","/lampen/",DEVICE_NAME,"/status");
        if(!strcmp(buffer,topic)) {
                char buffer[10];
                snprintf(buffer,length+1,"%s",payload);
                if(!strcmp(buffer,"1")) {
                        dimmer_on();
                        return;
                }
                if(!strcmp(buffer,"0")) {
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
