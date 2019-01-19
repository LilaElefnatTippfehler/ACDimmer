#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include "config.h"

#define DEVICE_NAME "DimmerTest"
#define EXPPERIOD 10000  //Expected Period
#define ZC D5
#define PWM D6
#define PERIODBUFFER 30


void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void zeroCross();
void getPeriod();
void handlerTimer();

unsigned long lastZC = 0;
volatile unsigned long thisZC = 0;
long periodBuffer[PERIODBUFFER];
long period = 0;
volatile long readings = 0;
volatile uint8_t trigger = 0;
volatile uint32_t tLow = 0;
volatile int duty = 0;

WiFiClient espClient;
PubSubClient client(MQTT_IP,MQTT_PORT,callback,espClient);


void setup() {
        //Init Interrupt
        pinMode(ZC, INPUT_PULLUP);
        pinMode(PWM, OUTPUT);
        attachInterrupt(digitalPinToInterrupt(ZC), zeroCross, RISING);
        timer1_isr_init();
        timer1_attachInterrupt(handlerTimer);
        timer1_enable(TIM_DIV16,TIM_EDGE,TIM_SINGLE);

        Serial.begin(115200);
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID,WIFI_PASS);
        while (WiFi.status() != WL_CONNECTED) {
                delay(500);
                Serial.print(".");
        }
        while(!Serial);

        Serial.print("Connected, IP address: ");
        Serial.println(WiFi.localIP());

}

void loop() {
        if (!client.connected()) {
                reconnect();
        }
        client.loop();

        if(trigger == 1) {
                trigger = 0;
                getPeriod();
                tLow = period * (100-duty) / 100;
        }

        if(readings>=40) {
                Serial.print(" P: "); Serial.println(period,DEC);
                Serial.print(" duty: "); Serial.println(duty,DEC);
                Serial.print(" timer: "); Serial.println(tLow,DEC);
                readings = 0;
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

void getPeriod(){

        trigger = 0;
        if(lastZC == 0) {
                lastZC = thisZC;
                return;
        }
        if(lastZC >= thisZC) {
                period = EXPPERIOD;
                return;
        }
        if((thisZC - lastZC)>=(EXPPERIOD+200)||(thisZC - lastZC)<=(EXPPERIOD-200)) {
                periodBuffer[readings%PERIODBUFFER] = periodBuffer[readings%PERIODBUFFER-1];
        }else{
                periodBuffer[readings%PERIODBUFFER] = thisZC - lastZC;
        }
        lastZC = thisZC;
        if(!(readings%PERIODBUFFER)) {
                for(int i = 0; i<PERIODBUFFER; i++) {
                        period += periodBuffer[i];
                }
                period = period/PERIODBUFFER;
        }

}

void callback(char* topic, byte* payload, unsigned int length){
        char buffer[100];
        snprintf(buffer,length+1,"%s",payload);
        duty = atoi(buffer);
}

void zeroCross(){
        thisZC = micros();
        timer1_disable();
        digitalWrite(PWM, LOW);
        timer1_enable(TIM_DIV16,TIM_EDGE,TIM_SINGLE);
        timer1_write(tLow*5);
        readings++;
        trigger = 1;
}

void handlerTimer(){
        digitalWrite(PWM, HIGH);

}
