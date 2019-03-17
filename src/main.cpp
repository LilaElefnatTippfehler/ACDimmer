
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

#include "ACDimmer.h"
#include "config.h"
#include "touchAutomat.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>

#define TOUCH D7
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
void LSinit();
void LSmove(int duty, int time_ms);

//---------Voice Commands Key Words--------
const String On[] = {"ein", "an", "auf"};
const uint8_t numOn = 3;
const String Off[] = {"aus", "ab"};
const uint8_t numOff = 2;

const String lvl[] = {"auf","zu"};
const uint8_t numLvl = 2;


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

void setup() {
        Serial.begin(115200);
        init_dimmer();

        LSinit();
        pinMode(TOUCH, INPUT);

        attachInterrupt(digitalPinToInterrupt(TOUCH), touchISR, CHANGE);
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        while (WiFi.status() != WL_CONNECTED) {
                delay(500);
                Serial.print(".");
        }
        while (!Serial)
                ;
        Serial.println("");
        Serial.print("Connected, IP address: ");
        Serial.println(WiFi.localIP());

        httpServer_ini();

        MQTTpub.attach(1.0, MQTTpubISR);
        checkTime.attach(30.0, timeISR);
        ShedPub.attach(60.0, shedPubISR);

        String ClientID = String(CLIENTID) + DEVICE_NAME;
        dimmer_move(50, 800);
        delay(800);
        dimmer_move(20, 800);
        delay(800);
        dimmer_move(50, 800);
        delay(800);
        dimmer_move(20, 800);
        delay(800);
        dimmer_set(0);
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
        touchAutomat();
        funWithFlags();

        MDNS.update();
}

void LSinit() {
        pinMode(LS, OUTPUT);
        analogWrite(LS, 0);
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
                client.subscribe("/lampen/ada/json", 1);
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
                if (last_val != dimmer_getDuty()) {
                        last_val = dimmer_getDuty();
                        return;
                }
                if (old_status != dimmer_status() || old_duty != dimmer_getDuty() || flag_ShedPub) {
                        flag_time = 1;

                        const int capacity = JSON_OBJECT_SIZE(3)+JSON_OBJECT_SIZE(3);
                        StaticJsonDocument<capacity> root; // New ArduinoJson 6 syntax
                        old_status = dimmer_status();
                        old_duty = dimmer_getDuty();
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

void funWithFlags() {
        if (flag_MQTTpub) {
                MQTTKeepTrack();
                flag_MQTTpub = 0;
                flag_ShedPub = 0;
        }
        if (flag_time) {
                static unsigned long turnedOn = 0;
                static int lastStatus = 0;
                if (lastStatus == dimmer_status()) {
                        lastStatus = dimmer_status();
                        if (dimmer_status() == 1) {
                                timeOn += millis() - turnedOn;
                                turnedOn = millis();
                        }
                } else {
                        if (dimmer_status() == 1) {
                                turnedOn = millis();
                        } else {
                                timeOn += millis() - turnedOn;
                        }
                        lastStatus = dimmer_status();
                }
                flag_time = 0;
        }
}

void callback(char *topic, byte *payload, unsigned int length) {
        if (!strcmp("/lampen/ada/json", topic)) {
                const int capacity = 2 * JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5);
                Serial.println("Adafruit Input");
                StaticJsonDocument<capacity> msg;
                DeserializationError err = deserializeJson(msg, payload);
                String data;
                if(!err) {

                        auto jsondata = msg["data"];
                        const char *value = jsondata["value"];
                        data = String(value);
                }else{
                        Serial.print("Couldnt parse Json Object from: ");
                        Serial.println(topic);
                        Serial.print("Error: ");
                        Serial.println(err.c_str());

                }
                Serial.print("Payload: ");
                Serial.println(data);
                uint8_t i = 0;
                while (i < numOn) {
                        if (!data.compareTo(On[i])) {
                                dimmer_on();
                                return;
                        }
                        i++;
                }
                i = 0;
                while (i < numOff) {
                        if (!data.compareTo(Off[i])) {
                                dimmer_off();
                                return;
                        }
                        i++;
                }

                if (data.startsWith(DEVICE_NAME)) {
                        data.remove(0, strlen(DEVICE_NAME));
                        data.trim();
                        uint8_t i = 0;
                        while (i < numOn) {
                                if (data.endsWith(On[i]) && data.startsWith(On[i])) {
                                        dimmer_on();
                                        return;
                                }
                                i++;
                        }
                        i = 0;
                        while (i < numOff) {
                                if (data.endsWith(Off[i]) && data.startsWith(Off[i])) {
                                        dimmer_off();
                                        return;
                                }
                                i++;
                        }
                        i=0;
                        while(i<numLvl) {
                                if(data.startsWith(lvl[i])) {
                                        data.remove(0,lvl[i].length());

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
        sprintf(buffer, "%s%s%s", "/lampen/", DEVICE_NAME, "/status");
        if (!strcmp(buffer, topic)) {
                char buffer[10];
                snprintf(buffer, length + 1, "%s", payload);
                if (!strcmp(buffer, "1")) {
                        dimmer_on();
                        return;
                }
                if (!strcmp(buffer, "0")) {
                        dimmer_off();
                        return;
                }
        }
        sprintf(buffer, "%s%s%s", "/lampen/", DEVICE_NAME, "/pwm");
        if (!strcmp(buffer, topic)) {
                int duty;
                char buffer[10];
                snprintf(buffer, length + 1, "%s", payload);
                duty = atoi(buffer);
                dimmer_move(duty);
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
