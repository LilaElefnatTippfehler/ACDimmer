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
unsigned long lastZC = 0;
unsigned long thisZC = 0;
long periodBuffer[PERIODBUFFER];
long period = 0;
long readings = 0;
uint32_t tLow = 0;
int duty_save = 0;
int duty_goal = 0;
int direction = -1;
unsigned long steps = 0;


Ticker tick;

void init_dimmer(){
        pinMode(ZC, INPUT_PULLUP);
        pinMode(PWM, OUTPUT);


        timer1_isr_init();
        timer1_attachInterrupt(handlerTimer);
        timer1_enable(TIM_DIV1,TIM_EDGE,TIM_SINGLE);


        attachInterrupt(digitalPinToInterrupt(ZC), initPeriod, FALLING);
        while(!getPeriod());
        detachInterrupt(digitalPinToInterrupt(ZC));
        attachInterrupt(digitalPinToInterrupt(ZC), zeroCross, FALLING);
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
        updateTime(duty);
}

void dimmer_on(){
        updateTime(duty_save);
}

void dimmer_off(){
        updateTime(1);
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


        Serial.println("In dimmer_move");

        if(duty_save > duty) {
                direction = 0;
                steps = DIMMSPEED / (duty_save-duty);
                Serial.print("Going down steps ");
                Serial.println(steps);
        }
        if(duty_save < duty) {
                direction = 1;
                steps = DIMMSPEED / (duty-duty_save);
                Serial.print("Going up  steps ");
                Serial.println(steps);
        }

        duty_goal = duty;
        tick.attach_ms(steps, tickHandler);
        Serial.println("After Interrupt");
        Serial.print("duty ");
        Serial.println(duty);
        Serial.print("duty_save ");
        Serial.println(duty_save);
        Serial.print("Direction ");
        Serial.println(direction);

}

void dimmer(){
        if(flag_ZC) {
                timer1_write(tLow*80);
                flag_ZC = FALSE;
        }
        if(flag_timer1) {
                digitalWrite(PWM, HIGH);
                delayMicroseconds(12);
                digitalWrite(PWM, LOW);
                flag_timer1 = FALSE;
        }
        if(flag_ticker) {
                if(direction == 0) {
                        dimmer_set(duty_save-1);
                }
                if(direction == 1) {
                        dimmer_set(duty_save+1);
                }
                flag_ticker = FALSE;
                if(duty_goal == duty_save) tick.detach();

        }

}

void initPeriod(){
        thisZC = micros();
        readings++;
}

void zeroCross(){
        flag_ZC = TRUE;

}
void tickHandler(){
        //Serial.println("in ISR ");
        flag_ticker = TRUE;
}

void handlerTimer(){
        flag_timer1 = TRUE;
}
