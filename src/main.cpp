
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
#include "homie.hpp"

#define SERIAL true

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
void dimmerMove(int duty, int time = 0);
void lampMove(int duty, int time = 0);
void dimmerSet(int duty);
void lampSet(int duty);
void dimmerOnOff(bool onoff);
void lampOnOff(bool onoff);
void dimmerMQTTUpdate(int time = 1000);
void lampMQTTUpdate(int time = 1000);
void announce();

//---------Announce Arrays----------------
//You'll also have to change announce function
const String data[] = {"status","brightness","ontime"};
const String cmd[] = {"status","brightness"};


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
Homie homieCTRL = Homie(&client);

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
                if(SERIAL) Serial.print(".");
        }
        if(SERIAL) Serial.println("");
        if(SERIAL) Serial.print("Connected, IP address: ");
        if(SERIAL) Serial.println(WiFi.localIP());

        httpServer_ini();
        HomieDevice homieDevice = HomieDevice(DEVICE_NAME, "Nachttisch", WiFi.localIP().toString().c_str(),
                                              WiFi.macAddress().c_str(), FW_NAME, FW_VERSION,
                                              "esp8266", "60");

        HomieNode stringLights = HomieNode("string-lights", "String Lights", "LEDDimmer");
        HomieNode dimmer = HomieNode("dimmer", "Dimmer", "ACDimmer");
        HomieProperties brightness = HomieProperties("brightness", "Brightness",
                                                     true, true, "%",
                                                     homie::float_t, "0:1");
        HomieProperties power = HomieProperties("power", "Power",
                                                true, true, "",
                                                homie::boolean_t, "");

        dimmer.addProp(brightness);
        dimmer.addProp(power);
        stringLights.addProp(brightness);
        stringLights.addProp(power);
        homieDevice.addNode(dimmer);
        homieDevice.addNode(stringLights);
        homieCTRL.setDevice(homieDevice);
        if(SERIAL) {
                Serial.println(dimmer.toString().c_str());
                Serial.println(brightness.toString().c_str());
                Serial.println(stringLights.toString().c_str());
                Serial.println(homieDevice.toString().c_str());
                Serial.println(homieCTRL.getDevice().toString().c_str());
        }

        //MQTTpub.attach(1.0, MQTTpubISR);
        //checkTime.attach(30.0, timeISR);
        ShedPub.once(50.0, shedPubISR);
        //ShedPub.attach(50.0, shedPubISR);

        ClientID = String(CLIENTID) + DEVICE_NAME;
        homieCTRL.connect(ClientID.c_str(), MQTT_USR, MQTT_PW);
        changeLvl("move_ms",50, 800);
        changeLvl("move_ms",0, 800);

}

void loop() {
        if (!homieCTRL.connected()) {
                unsigned long now = millis();
                if (now - lastReconnectAttempt > 5000) {
                        lastReconnectAttempt = now;
                        // Attempt to reconnect
                        if (reconnect()) {
                                lastReconnectAttempt = 0;
                        }
                }
        }
        homieCTRL.loop();
        httpServer.handleClient();
        funWithFlags();

        MDNS.update();
}

void httpServer_ini() {
        char buffer[100];
        sprintf(buffer, "%s", DEVICE_NAME);
        MDNS.begin(buffer);
        httpUpdater.setup(&httpServer, update_path, update_username, update_password);
        httpServer.begin();
        MDNS.addService("http", "tcp", 80);
        if(SERIAL) Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your "
                                 "browser and login with username '%s' and password '%s'\n",
                                 buffer, update_path, update_username, update_password);
        //------
}

boolean reconnect() {
        // Loop until we're reconnected
        return homieCTRL.connect(ClientID.c_str(), MQTT_USR, MQTT_PW);
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

void heartBeat(){
        if(flag_ShedPub) {
                long time = millis() / 1000;
                string topic = "homie/" + string(DEVICE_NAME) + "/$stats/uptime";
                char payload[20];
                sprintf(payload, "%ld", time);
                ShedPub.once(60.0, shedPubISR);
                client.publish(topic.c_str(), payload,true);
                topic = "homie/" + string(DEVICE_NAME) + "/$stats/interval";
                client.publish(topic.c_str(), "60",true);
                flag_ShedPub = 0;
        }
}

void funWithFlags() {
        heartBeat();
        if (flag_MQTTpub) {
                //MQTTKeepTrack();
                heartBeat();
                flag_MQTTpub = 0;

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
        string topicString = string(topic);
        if(SERIAL) Serial.println(topicString.c_str());

        std::size_t found = topicString.find("dimmer/brightness/set");
        if(found!=std::string::npos) {
                char buffer[10];
                snprintf(buffer, length + 1, "%s", payload);
                double duty_f = atof(buffer);
                dimmerMove((int)(duty_f*100.0));
        }

        found = topicString.find("string-lights/brightness/set");
        if(found!=std::string::npos) {
                char buffer[10];
                snprintf(buffer, length + 1, "%s", payload);
                double duty_f = atof(buffer);
                lampMove((int)(duty_f*100.0));
        }

        found = topicString.find("dimmer/power/set");
        if(found!=std::string::npos) {
                char buffer[6];
                snprintf(buffer, length + 1, "%s", payload);
                if (!strcmp(buffer, "true")) {
                        changeLvl("on");
                        return;
                }
                if (!strcmp(buffer, "false")) {
                        changeLvl("off");
                        return;
                }
        }

        found = topicString.find("string-lights/power/set");
        if(found!=std::string::npos) {
                char buffer[6];
                snprintf(buffer, length + 1, "%s", payload);
                if (!strcmp(buffer, "true")) {
                        lampMove(100);
                        return;
                }
                if (!strcmp(buffer, "false")) {
                        lampMove(0);
                        return;
                }
        }

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
                                if(SERIAL) Serial.println(lum);
                                if((dimmer->getStatus() == 0) && (lum > 11)) {
                                        changeLvl("on");
                                }else{
                                        if((dimmer->getStatus() == 1) && (lum <= 11)) {
                                                changeLvl("off");
                                        }else{
                                                changeLvl("move",lum);
                                        }
                                }

                        }
                }else{
                        if(SERIAL) Serial.print("Couldnt parse Json Object from: ");
                        if(SERIAL) Serial.println(topic);
                        if(SERIAL) Serial.print("Error: ");
                        if(SERIAL) Serial.println(err.c_str());

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
                        if(SERIAL) {
                                Serial.println(hours); Serial.println(minutes);
                                Serial.println(seconds); Serial.println("");
                        }
                }else{
                        if(SERIAL) Serial.print("Couldnt parse Json Object from: ");
                        if(SERIAL) Serial.println(topic);
                        if(SERIAL) Serial.print("Error: ");
                        if(SERIAL) Serial.println(err.c_str());
                }

                client.unsubscribe(buffer);
        }

}

void changeLvl(String cmd,int duty, int time){
        if(!cmd.compareTo("move")) {
                dimmerMove(duty);
                lampMove(duty);
        }else if(!cmd.compareTo("move_ms")) {
                dimmerMove(duty,time);
                lampMove(duty,time);
        }else if(!cmd.compareTo("set")) {
                dimmerSet(duty);
                lampSet(duty);
        }else if(!cmd.compareTo("on")) {
                dimmerOnOff(true);
                lampOnOff(true);
        }else if(!cmd.compareTo("off")) {
                dimmerOnOff(false);
                lampOnOff(false);
        }
}

void dimmerMove(int duty, int time){
        if(time == 0) {
                dimmer->move(duty);
                Serial.println("move");
        }else{
                dimmer->move(duty,time);
                Serial.println("move time");
        }
        dimmerMQTTUpdate();
}
void lampMove(int duty, int time){
        if(time == 0) {
                lamps.move(duty);
                Serial.println("move");
        }else{
                lamps.move(duty,time);
                Serial.println("move time");
        }
        lampMQTTUpdate();
}
void dimmerSet(int duty){
        dimmer->set(duty);
        dimmerMQTTUpdate();
}
void lampSet(int duty){
        lamps.set(duty);
        lampMQTTUpdate();
}
void dimmerOnOff(bool onoff){
        if(onoff) {
                dimmer->on();
        }else{
                dimmer->off();
        }
        dimmerMQTTUpdate();
}
void lampOnOff(bool onoff){
        if(onoff) {
                lamps.on();
        }else{
                lamps.off();
        }
        lampMQTTUpdate();
}
void dimmerMQTTUpdate(int time){
        if(homieCTRL.connected()) {
                delay(time);

                char newDuty[6];
                float duty_f = (float) dimmer->getDuty() / 100.0;
                //itoa(dimmer->getDuty(),newDuty,10);
                sprintf(newDuty,"%.3f",duty_f);
                string power = "false";
                if(dimmer->getStatus()) {
                        power = "true";
                }
                client.publish("homie/nachttisch/dimmer/brightness",newDuty,true);
                client.publish("homie/nachttisch/dimmer/power",power.c_str(),true);
        }
}
void lampMQTTUpdate(int time){
        if(homieCTRL.connected()) {
                delay(time);

                char newDuty[6];
                float duty_f = (float) lamps.getDuty() / 100.0;
                //itoa(lamps.getDuty(),newDuty,10);
                sprintf(newDuty,"%.3f",duty_f);
                string power = "false";
                if(lamps.getStatus()) {
                        power = "true";
                }
                client.publish("homie/nachttisch/string-lights/brightness",newDuty,true);
                client.publish("homie/nachttisch/string-lights/power",power.c_str(),true);
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
