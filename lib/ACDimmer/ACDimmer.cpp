#include "ACDimmer.h"

void zeroCross();
int getPeriod();
void handlerTimer();
void tickHandler();
void initPeriod();
void updateTime(int time);

uint8_t flag_ZC = 0;
uint8_t flag_timer1 = 0;
uint8_t flag_ticker = 0;
uint8_t flag_ini = 0;
uint8_t status = 2;
unsigned long lastZC = 0;
unsigned long thisZC = 0;
long periodBuffer[PERIODBUFFER];
long period = 0;
long readings = 1;
int trigger = 0;
uint32_t tLow = 0;
int duty_save = 0;
int duty_goal = 0;
int duty_old = 0;
int direction = -1;
unsigned long steps = 0;


Ticker tick;

void init_dimmer(){
        pinMode(ZC, INPUT_PULLUP);
        pinMode(PWM, OUTPUT);
        timer1_isr_init();
        timer1_attachInterrupt(handlerTimer);
        timer1_enable(TIM_DIV16,TIM_EDGE,TIM_SINGLE);
        attachInterrupt(digitalPinToInterrupt(ZC), initPeriod, FALLING);
        while(!getPeriod()) {
                dimmer();
        }
        //period = EXPPERIOD;       //Just for debugging
        detachInterrupt(digitalPinToInterrupt(ZC));
        attachInterrupt(digitalPinToInterrupt(ZC), zeroCross, FALLING);

        dimmer_set(50);
        dimmer_set(1);
}

int getPeriod(){

        int temp = 0;
        if(lastZC == 0) {
                lastZC = thisZC;
                return 0;
        }
        if(lastZC >= thisZC) {
                period = EXPPERIOD;
                return 0;
        }
        if((thisZC - lastZC)>=(EXPPERIOD+400)||(thisZC - lastZC)<=(EXPPERIOD-400)) {
                periodBuffer[readings%PERIODBUFFER] = periodBuffer[readings%PERIODBUFFER-1];
        }else{
                periodBuffer[readings%PERIODBUFFER] = thisZC - lastZC;
        }
        lastZC = thisZC;
        if(!(readings%PERIODBUFFER)) {
                for(int i = 0; i<PERIODBUFFER; i++) {
                        temp += periodBuffer[i];
                }
                period = temp/PERIODBUFFER;
                readings = 0;
                return 1;
        }
        return 0;

}

void dimmer_set(int duty){
        duty_save = duty;
        if(duty >= 100) duty_save = 100;
        if(duty <= 0) duty_save = 0;
        updateTime(duty_save);
}

void dimmer_on(){
        if(duty_old <= 20) {
                dimmer_move(100);
        }else{
                dimmer_move(duty_old);
        }
        status = 1;

}

void dimmer_off(){
        duty_old = duty_save;
        status = 0;
        dimmer_move(0);
}

void updateTime(int time){
        static int temp;
        if(time <=10 && temp >= 11) {
                temp = 10;
                detachInterrupt(digitalPinToInterrupt(ZC));
        } else {
                if(time >= 11 && temp <= 10) {
                        attachInterrupt(digitalPinToInterrupt(ZC), zeroCross, FALLING);
                }
                if(time >= 99) {
                        temp = 95;
                }else{
                        temp = time;
                }
        }

        tLow = period * (100-temp) / 100;
}

void dimmer_move(int duty){



        if(duty_save > duty) {
                direction = 0;
                steps = DIMMSPEED / (duty_save-duty);
        }
        if(duty_save < duty) {
                direction = 1;
                steps = DIMMSPEED / (duty-duty_save);
        }

        duty_goal = duty;
        tick.attach_ms(steps, tickHandler);

}

void dimmer_move(int duty, int time_ms){
        if(duty_save > duty) {
                direction = 0;
                steps = time_ms / (duty_save-duty);
        }
        if(duty_save < duty) {
                direction = 1;
                steps = time_ms / (duty-duty_save);
        }

        duty_goal = duty;
        tick.attach_ms(steps, tickHandler);
}
void dimmer(){
        if(flag_ini) {
                thisZC = micros();
                readings++;
                flag_ini = 0;
        }
        if(flag_ticker == 1) {
                Serial.println("in dimmer move");
                if(direction == 0) {
                        dimmer_set(duty_save-1);
                }
                if(direction == 1) {
                        dimmer_set(duty_save+1);
                }
                flag_ticker = 0;
                if(duty_goal == duty_save) tick.detach();
        }


}

boolean dimmer_ismoving(){
        Serial.print("duty_goal: "); Serial.println(duty_goal);
        Serial.print("duty_save: "); Serial.println(duty_save);
        if(duty_goal == duty_save) {
                Serial.println("false");
                return false;
        }else{
                Serial.println("true");
                return true;
        }
}

int dimmer_status(){
        if(duty_save <= 10) {
                status = 0;
        }
        if(duty_save >= 11) {
                status = 1;
        }
        return status;
}

void dimmer_up(){
        dimmer_set(duty_save+1);
}

void dimmer_down(){
        dimmer_set(duty_save-1);
}

int dimmer_getDuty(){
        return duty_save;
}

void initPeriod(){
        flag_ini = 1;
}

void zeroCross(){
        timer1_write(tLow*5);
        flag_ZC = 1;

}
void tickHandler(){
        flag_ticker = 1;
}

void handlerTimer(){
        if(flag_timer1) {
                digitalWrite(PWM, LOW);
                flag_timer1 = 0;
        }else{
                digitalWrite(PWM, HIGH);
                timer1_write(15*5);
                flag_timer1 = 1;
        }

}
