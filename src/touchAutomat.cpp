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
                        if(touchT==0) touchT = millis();
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
