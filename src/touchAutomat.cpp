#include "touchAutomat.h"

uint8_t flag_touchR = 0;
uint8_t flag_touchF = 0;

Ticker touchDimm;

void touchAutomat(){
        static unsigned long touchT = 0;
        static unsigned long betweenT = 0;
        static int status = 0;

        switch (status) {
        case 0:
                if(flag_touchR) {
                        Serial.println("DEBUG: case0 Touch");
                        touchT = millis();
                        status = 1;
                }
                break;
        case 1:
                Serial.println("DEBUG: case1 waiting for release 1");
                if((touchT+TOUCHTIME)<=millis()) {
                        Serial.println("DEBUG: case1 timeout");
                        status = 2;

                }

                if(flag_touchF) {
                        if((touchT+TOUCHTIME)>=millis()) {
                                Serial.println("DEBUG: case1 next touch");
                                status = 4;
                                betweenT = millis();
                                flag_touchR = 0;
                                flag_touchF = 0;

                        }
                        break;
                }
                break;
        case 2:
                Serial.println("DEBUG: case2 attach Ticker UP");
                touchDimm.attach_ms(50, touchDTISR_up);
                status = 3;
                touchT = 0;
                break;
        case 3:
                Serial.println("DEBUG: case3 waiting for release");
                if(flag_touchF) {
                        if((touchT + 1000)>= millis() && touchT) {
                                Serial.println("DEBUG: case3 touch released");
                                touchDimm.detach();
                                flag_touchR = 0;
                                flag_touchF = 0;
                                status = 0;
                        }
                        if(touchT==0) touchT = millis();
                }
                break;
        case 4:
                Serial.println("DEBUG: case4 waiting for second touch");
                if((betweenT + 500)<=millis()) {
                        Serial.println("DEBUG: case4 timeout");
                        if(dimmer_status()) {
                                dimmer_off();
                        }else{
                                dimmer_on();
                        }
                        status = 0;
                        break;
                }
                if(flag_touchR) {
                        Serial.println("DEBUG: case4 second touch");
                        status = 5;
                }


                break;
        case 5:
                Serial.println("DEBUG: case5 attach Ticker down");
                touchDimm.attach_ms(50, touchDTISR_down);
                status = 6;
                touchT = 0;
                break;
        case 6:
                if(flag_touchF) {
                        Serial.println("DEBUG: case6 waiting for release");
                        if((touchT + 1000)>= millis() && touchT) {
                                Serial.println("DEBUG: case6 touch released");
                                touchDimm.detach();
                                flag_touchR = 0;
                                flag_touchF = 0;
                                status = 0;
                                //Serial.println("Touch losgelassen");
                        }
                        if(touchT==0) touchT = millis();
                }
                break;
        }


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
