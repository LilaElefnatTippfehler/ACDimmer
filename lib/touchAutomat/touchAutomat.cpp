#include <Ticker.h>
#include <Arduino.h>
#include "touchAutomat.hpp"

bool touchAutomat::instanceFlag = false;
touchAutomat* touchAutomat::single = NULL;

touchAutomat::~touchAutomat(){
        instanceFlag = false;
}

void touchAutomat::init(void (*function)(String,int,int ), uint8_t touchPin){
        this->pin = touchPin;
        this->function = function;
        pinMode(touchPin, INPUT);
        attachInterrupt(digitalPinToInterrupt(touchPin), this->touchISR, CHANGE);
}

void touchAutomat::touchISR(){
        touchAutomat::instance()->Tisr();
}

touchAutomat* touchAutomat::instance(){
        if(!instanceFlag) {
                single = new touchAutomat();
                instanceFlag = true;
                return single;
        }
        static touchAutomat instance;
        return single;
}

void touchAutomat::Tisr(){
        touched(digitalRead(this->pin));
        if((this->deltaTime >= 30) && (this->deltaTime <= 500)) {
                switch(this->status) {
                case 0:
                        function("move",20,0);
                        this->status = 1;
                        break;
                case 1:
                        function("move",35,0);
                        this->status = 2;
                        break;
                case 2:
                        function("move",50,0);
                        this->status = 3;
                        break;
                case 3:
                        function("move",65,0);
                        this->status = 4;
                        break;
                case 4:
                        function("move",80,0);
                        this->status = 5;
                        break;
                case 5:
                        function("move",100,0);
                        break;
                }
        }
        if(this->deltaTime > 500) {
                if(this->status == 0) {
                        function("move",100,0);
                        this->status = 5;
                } else {
                        function("move",0,0);
                        this->status = 0;
                }
        }
}

void touchAutomat::touched(uint8_t state){
        static long old_time;
        if(state == HIGH) {
                old_time = millis();
                this->deltaTime = 0;
        } else{
                if(state == LOW) {
                        this->deltaTime = millis() - old_time;
                }
        }
}
