
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
#include <ArduinoJson.h>
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
void timeISR();
void funWithFlags();
void shedPubISR();

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

volatile uint8_t flag_time = 0;
volatile uint8_t flag_MQTTpub = 0;
volatile uint8_t flag_ShedPub = 0;
int old_status = 0;
int old_duty = 0;
unsigned long timeOn = 0;

ESP8266HTTPUpdateServer httpUpdater;
ESP8266WebServer httpServer(80);
Ticker MQTTpub;
Ticker checkTime;
Ticker ShedPub;
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

        MQTTpub.attach(1.0, MQTTpubISR);
        checkTime.attach(1.0,timeISR);
        ShedPub.attach(60.0,shedPubISR);

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
        funWithFlags();

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
                String clientId = "ESP8266Client-";
                clientId += DEVICE_NAME;
                // Attempt to connect
                if (client.connect(clientId.c_str(),MQTT_USR,MQTT_PW)) {
                        Serial.print("connected as ");
                        Serial.print(clientId.c_str()), Serial.println("");
                        char buffer[100];
                        sprintf(buffer,"%s%s%s","/lampen/",DEVICE_NAME,"/status");
                        client.subscribe(buffer,1);
                        sprintf(buffer,"%s%s%s","/lampen/",DEVICE_NAME,"/pwm");
                        client.subscribe(buffer,1);
                        sprintf(buffer,"%s%s","/",DEVICE_NAME);
                        client.subscribe(buffer,1);
                        client.subscribe("/lampen/ada",1);

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
        static int last_val = 0;
        if(client.connected()) {
                if(last_val != dimmer_getDuty()) {
                        last_val = dimmer_getDuty();
                        return;
                }
                if(old_status != dimmer_status()||old_duty != dimmer_getDuty()||flag_ShedPub) {
                        const int capacity = JSON_OBJECT_SIZE(3)+JSON_OBJECT_SIZE(3);
                        StaticJsonBuffer<capacity> jb;
                        JsonObject& root = jb.createObject();
                        const int capacityT = JSON_OBJECT_SIZE(3);
                        StaticJsonBuffer<capacityT> jbT;
                        JsonObject& time = jbT.createObject();
                        old_status = dimmer_status();
                        old_duty = dimmer_getDuty();
                        time["hours"].set(timeOn/1000/60/60);
                        time["minutes"].set(timeOn/1000/60%60);
                        time["seconds"].set(timeOn/1000%60);

                        root["percentage"].set(old_duty);
                        root["status"].set(old_status);
                        root["On time"].set(time);
                        String topic = "/" + String(DEVICE_NAME);
                        char* buffer = (char*) malloc(topic.length()+1);
                        topic.toCharArray(buffer, topic.length()+1);
                        String output;
                        root.printTo(output);
                        uint8_t* buffer2 = (uint8_t*) malloc(output.length()+1);
                        output.getBytes(buffer2, output.length()+1);
                        client.publish(buffer, buffer2,output.length()+1,true);

                }
        }
}

void funWithFlags(){
        if(flag_MQTTpub) {
                MQTTKeepTrack();
                flag_MQTTpub = 0;
                flag_ShedPub = 0;
        }
        if(flag_time) {
                static unsigned long turnedOn = 0;
                static int lastStatus = 0;
                if(lastStatus == dimmer_status()) {
                        lastStatus = dimmer_status();
                        if(dimmer_status() == 1) {
                                timeOn += millis() - turnedOn;
                                turnedOn = millis();
                        }
                } else{
                        if(dimmer_status() == 1) {
                                turnedOn = millis();
                        }else{
                                timeOn += millis() - turnedOn;
                        }
                        lastStatus = dimmer_status();
                }
                flag_time = 0;

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
        sprintf(buffer,"%s%s","/",DEVICE_NAME);
        if(!strcmp(buffer,topic)) {
                const int capacity = JSON_OBJECT_SIZE(3)+JSON_OBJECT_SIZE(3);
                StaticJsonBuffer<capacity> jb;
                JsonObject& msg = jb.parseObject(payload);
                if(msg.success()) {
                        auto time = msg["On time"];
                        auto hours = time["hours"].as<unsigned long>();
                        auto minutes = time["minutes"].as<unsigned long>();
                        auto seconds = time["seconds"].as<unsigned long>();
                        timeOn += (seconds+ minutes * 60 + hours * 60 * 60) * 1000;
                        Serial.println(hours); Serial.println(minutes); Serial.println(seconds); Serial.println("");
                }else{
                        Serial.print("Couldnt parse Json Object from: "); Serial.println(topic);
                }

                client.unsubscribe(buffer);
        }



}

void MQTTpubISR(){
        flag_MQTTpub = 1;
}

void timeISR(){
        flag_time = 1;
}

void shedPubISR(){
        flag_ShedPub = 1;
}
