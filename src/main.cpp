
/*
   YOU'LL NEED TO CHANGE MQTT_MAX_PACKET_SIZE IN PubSubClient.h to 512!
 #define MQTT_MAX_PACKET_SIZE 512

   Device is connected to MQTT Server.
   MQTT Topic structure I used is:
   /lampen/DEVICE_NAME/
                   /status    Gets turn on/off commands from here (from other
   clients) /pwm       Gets the level of brightness from here 0-100

   /lampen/ada/json  Bridged AdafruitIO feed here. Commands come from Google
   Assistant via IFTTT

   /DEVICE_NAME      In these topics every device publishes its
   status/brightness/sensors/whatever

   Commands look like this:
   IFTT_ACTIVATION_STRING KEYWORD                        This one turns on/off
   all lamps that subscibed to /ada/lampen KEYWORD out of On[] or Off[]

   IFTT_ACTIVATION_STRING DEVICE_NAME KEYWORD            This turns on only the
   device with the name DEVICE_NAME. KEYWORD out of On[] or Off[] KEYWORD must be
   the first and last word after DEVICE_NAME


   IFTT_ACTIVATION_STRING DEVICE_NAME KEYWORD NUMBER     Dimming the lamp to NUMBER percent. everything after NUMBER will be ignored
   KEYWORD out of lvl[]     NUMBER 0-100

   if there is the same word in On[]/Off[] and lvl[] the last command wont work if it is said as first word after DEVICE_NAME
   and as last word of the command.

 */

#include "ACDimmer.hpp"
#include "LEDString.hpp"
#include "config.h"
#include "touchAutomat.hpp"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>

#define LAMPS D1
#define TOUCH D2
#define ZC D5
#define PWM D6
#define CLIENTID "ESP8266Client-"

void callback(char *topic, byte *payload, unsigned int length);
boolean reconnect();
void MQTTKeepTrack();
void MQTTpubISR();
void httpServer_ini();
void finishedStartUp(int speed);
void timeISR();
void funWithFlags();
void shedPubISR();
void changeLvl(String cmd, int duty = 0, int time = 0);
void announce();

//---------Announce Arrays----------------
//You'll also have to change announce function
const String data[] = {"status","brightness","ontime"};
const String cmd[] = {"status","brightness"};


const char *host = "esp8266-";
const char *update_path = "/firmware";
const char *update_username = USERNAME;
const char *update_password = PASSWORD;

volatile uint8_t flag_time = 0;
volatile uint8_t flag_MQTTpub = 0;
volatile uint8_t flag_ShedPub = 0;
int old_status = 0;
int old_duty = 0;
unsigned long timeOn = 0;
unsigned long lastReconnectAttempt = 0;
String ClientID;
unsigned long cycleTime = 0;

ESP8266HTTPUpdateServer httpUpdater;
ESP8266WebServer httpServer(80);
Ticker MQTTpub;
Ticker checkTime;
Ticker ShedPub;
WiFiClient espClient;
PubSubClient client(MQTT_IP, MQTT_PORT, callback, espClient);
LEDString lamps(LAMPS);
touchAutomat *ta = touchAutomat::instance();
ACDimmer *dimmer = ACDimmer::instance();

void setup() {
        Serial.begin(115200);
        while (!Serial);
        //init_dimmer();
        dimmer->init(ZC,PWM);
        lamps.init();
        ta->init(changeLvl,TOUCH);
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        while (WiFi.status() != WL_CONNECTED) {
                delay(500);
                Serial.print(".");
        }
        Serial.println("");
        Serial.print("Connected, IP address: ");
        Serial.println(WiFi.localIP());

        httpServer_ini();

        MQTTpub.attach(1.0, MQTTpubISR);
        checkTime.attach(30.0, timeISR);
        ShedPub.attach(60.0, shedPubISR);

        ClientID = String(CLIENTID) + DEVICE_NAME;
        changeLvl("move_ms",50, 800);
        delay(800);
        changeLvl("move_ms",20, 800);
        delay(800);
        changeLvl("move_ms",50, 800);
        delay(800);
        changeLvl("move_ms",0, 800);
        delay(800);
}

void loop() {
        if (!client.connected()) {
                unsigned long now = millis();
                if (now - lastReconnectAttempt > 5000) {
                        lastReconnectAttempt = now;
                        // Attempt to reconnect
                        if (reconnect()) {
                                lastReconnectAttempt = 0;
                        }
                }
        }
        client.loop();
        httpServer.handleClient();
        // dimmer();
        //touchAutomat();
        funWithFlags();

        MDNS.update();
}

void httpServer_ini() {
        char buffer[100];
        sprintf(buffer, "%s%s", host, DEVICE_NAME);
        MDNS.begin(buffer);
        httpUpdater.setup(&httpServer, update_path, update_username, update_password);
        httpServer.begin();
        MDNS.addService("http", "tcp", 80);
        Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your "
                      "browser and login with username '%s' and password '%s'\n",
                      buffer, update_path, update_username, update_password);
        //------
}

boolean reconnect() {
        // Loop until we're reconnected
        if (client.connect(ClientID.c_str(), MQTT_USR, MQTT_PW)) {
                char buffer[100];
                sprintf(buffer, "%s%s%s", "/lampen/", DEVICE_NAME, "/status");
                client.subscribe(buffer, 1);
                sprintf(buffer, "%s%s%s", "/lampen/", DEVICE_NAME, "/pwm");
                client.subscribe(buffer, 1);
                sprintf(buffer, "%s%s", "/", DEVICE_NAME);
                client.subscribe(buffer, 1);
                sprintf(buffer, "%s%s%s", "/actu/", DEVICE_NAME, "/cmd");
                client.subscribe(buffer, 1);
                announce();

        } else {
                Serial.print("MQTT conncetion failed, rc=");
                Serial.print(client.state());
                Serial.println(" try again in 5 seconds");
        }
        return client.connected();
}

void MQTTKeepTrack() {
        static int last_val = 0;
        if (client.connected()) {
                if (last_val != dimmer->getDuty()) {
                        last_val = dimmer->getDuty();
                        return;
                }
                if (old_status != dimmer->getStatus() || old_duty != dimmer->getDuty() || flag_ShedPub) {
                        flag_time = 1;

                        const int capacity = JSON_OBJECT_SIZE(3)+JSON_OBJECT_SIZE(3);
                        StaticJsonDocument<capacity> root; // New ArduinoJson 6 syntax
                        old_status = dimmer->getStatus();
                        old_duty = dimmer->getDuty();
                        root["level"].set(old_duty);

                        root["status"].set(old_status);
                        JsonObject time = root.createNestedObject("On time");
                        time["hours"].set(timeOn / 1000 / 60 / 60);
                        time["minutes"].set(timeOn / 1000 / 60 % 60);
                        time["seconds"].set(timeOn / 1000 % 60);

                        String topic = "/" + String(DEVICE_NAME);
                        char* buffer = (char*) malloc(topic.length()+1);
                        topic.toCharArray(buffer, topic.length()+1);
                        String output = "";
                        serializeJson(root, output); // New ArduinoJson 6 syntax
                        uint8_t* buffer2 = (uint8_t*) malloc(output.length()+1);
                        output.getBytes(buffer2, output.length()+1);
                        client.publish(buffer, buffer2,output.length()+1,true);

                        free(buffer);
                        free(buffer2);
                }
        }
}

void announce(){
        if(client.connected()) {
                const int capacity = JSON_ARRAY_SIZE(2) + JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(8); //hiher memory use...
                StaticJsonDocument<capacity> doc;
                JsonObject root = doc.to<JsonObject>();
                Serial.println(root.memoryUsage());
                root["device"] = String(DEVICE_NAME);
                root["ip"] = WiFi.localIP();
                Serial.println(WiFi.localIP());
                JsonArray cmdJ = root.createNestedArray("cmd");
                for(int i = 0; i<2; i++) {
                        Serial.print("cmd: ");
                        Serial.println(cmdJ.add(cmd[i]));
                }
                JsonArray dataJ = root.createNestedArray("data");
                for(int j = 0; j<3; j++) {
                        Serial.print("data: ");
                        Serial.println(dataJ.add(data[j]));
                }
                String output;
                serializeJson(root, output);
                uint8_t* buffer = (uint8_t*) malloc(output.length()+1);
                output.getBytes(buffer, output.length()+1);
                client.publish("/announce", buffer,output.length()+1);
                free(buffer);

                client.subscribe("/announce/fetch",1);
        }
}

void funWithFlags() {
        if (flag_MQTTpub) {
                MQTTKeepTrack();
                flag_MQTTpub = 0;
                flag_ShedPub = 0;
        }
        if (flag_time) {
                static unsigned long turnedOn = 0;
                static int lastStatus = 0;
                if (lastStatus == dimmer->getStatus()) {
                        lastStatus = dimmer->getStatus();
                        if (dimmer->getStatus() == 1) {
                                timeOn += millis() - turnedOn;
                                turnedOn = millis();
                        }
                } else {
                        if (dimmer->getStatus() == 1) {
                                turnedOn = millis();
                        } else {
                                timeOn += millis() - turnedOn;
                        }
                        lastStatus = dimmer->getStatus();
                }
                flag_time = 0;
        }
}

void callback(char *topic, byte *payload, unsigned int length) {
        char buffer[100];
        sprintf(buffer, "%s%s%s", "/actu/", DEVICE_NAME, "/cmd");
        if (!strcmp(buffer, topic)) {
                const int capacity = 2 * JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5);
                StaticJsonDocument<capacity> msg;
                DeserializationError err = deserializeJson(msg, payload);
                String data;
                if(!err) {

                        auto cmd = msg["cmd"];
                        if(cmd.containsKey("brightness")) {
                                uint8_t lum = cmd["brightness"];
                                int duty = lum/255*100;
                                Serial.println(lum);
                                Serial.println(duty);
                                changeLvl("move",duty);
                        }
                }else{
                        Serial.print("Couldnt parse Json Object from: ");
                        Serial.println(topic);
                        Serial.print("Error: ");
                        Serial.println(err.c_str());

                }
        }
        sprintf(buffer, "%s%s%s", "/lampen/", DEVICE_NAME, "/status");
        if (!strcmp(buffer, topic)) {
                char buffer[10];
                snprintf(buffer, length + 1, "%s", payload);
                if (!strcmp(buffer, "1")) {
                        changeLvl("on");
                        return;
                }
                if (!strcmp(buffer, "0")) {
                        changeLvl("off");
                        return;
                }
        }
        sprintf(buffer, "%s%s%s", "/lampen/", DEVICE_NAME, "/pwm");
        if (!strcmp(buffer, topic)) {
                int duty;
                char buffer[10];
                snprintf(buffer, length + 1, "%s", payload);
                duty = atoi(buffer);
                changeLvl("move",duty);
        }
        sprintf(buffer,"%s%s","/",DEVICE_NAME);
        if(!strcmp(buffer,topic)) {
                const int capacity = JSON_OBJECT_SIZE(3)+JSON_OBJECT_SIZE(3);
                StaticJsonDocument<capacity> msg;
                DeserializationError err = deserializeJson(msg, payload);
                if(!err) {
                        auto time = msg["On time"];
                        auto hours = time["hours"].as<unsigned long>();
                        auto minutes = time["minutes"].as<unsigned long>();
                        auto seconds = time["seconds"].as<unsigned long>();
                        timeOn += (seconds+ minutes * 60 + hours * 60 * 60) * 1000;
                        Serial.println(hours); Serial.println(minutes); Serial.println(seconds); Serial.println("");
                }else{
                        Serial.print("Couldnt parse Json Object from: "); Serial.println(topic);
                        Serial.print("Error: ");
                        Serial.println(err.c_str());
                }

                client.unsubscribe(buffer);
        }

        if(!strcmp("/announce/fetch",topic)) announce();
}

void changeLvl(String cmd,int duty, int time){
        if(!cmd.compareTo("move")) {
                dimmer->move(duty);
                lamps.move(duty);
        }else if(!cmd.compareTo("move_ms")) {
                dimmer->move(duty,time);
                lamps.move(duty,time);
        }else if(!cmd.compareTo("set")) {
                dimmer->set(duty);
                lamps.set(duty);
        }else if(!cmd.compareTo("on")) {
                dimmer->on();
                lamps.on();
        }else if(!cmd.compareTo("off")) {
                dimmer->off();
                lamps.off();
        }
}

void MQTTpubISR() {
        flag_MQTTpub = 1;
}

void timeISR() {
        flag_time = 1;
}

void shedPubISR() {
        flag_ShedPub = 1;
}
