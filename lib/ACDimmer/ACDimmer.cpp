#include "ACDimmer.h"

namespace ACDIMMER
{
void zeroCross();
int getPeriod();
void handlerTimer();
void tickHandler();
void initPeriod();
void updateTime(int time);

uint8_t flag_timer1 = 0;
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
}




void init_dimmer() {
        pinMode(ZC, INPUT_PULLUP);
        pinMode(PWM, OUTPUT);
        timer1_isr_init();
        timer1_attachInterrupt(ACDIMMER::handlerTimer);
        timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
        attachInterrupt(digitalPinToInterrupt(ZC), ACDIMMER::initPeriod, FALLING);
        while (!ACDIMMER::getPeriod());
        // period = EXPPERIOD;       //Just for debugging
        detachInterrupt(digitalPinToInterrupt(ZC));
        attachInterrupt(digitalPinToInterrupt(ZC), ACDIMMER::zeroCross, FALLING);

        dimmer_set(50);
        dimmer_set(1);
}

int ACDIMMER::getPeriod() {

        int temp = 0;
        if (ACDIMMER::lastZC == 0) {
                ACDIMMER::lastZC = ACDIMMER::thisZC;
                return 0;
        }
        if (ACDIMMER::lastZC >= ACDIMMER::thisZC) {
                ACDIMMER::period = EXPPERIOD;
                return 0;
        }
        if ((ACDIMMER::thisZC - ACDIMMER::lastZC) >= (EXPPERIOD + 400) || (ACDIMMER::thisZC - ACDIMMER::lastZC) <= (EXPPERIOD - 400)) {
                ACDIMMER::periodBuffer[ACDIMMER::readings % PERIODBUFFER] = ACDIMMER::periodBuffer[ACDIMMER::readings % PERIODBUFFER - 1];
        } else {
                ACDIMMER::periodBuffer[ACDIMMER::readings % PERIODBUFFER] = ACDIMMER::thisZC - ACDIMMER::lastZC;
        }
        ACDIMMER::lastZC = ACDIMMER::thisZC;
        if (!(ACDIMMER::readings % PERIODBUFFER)) {
                for (int i = 0; i < PERIODBUFFER; i++) {
                        temp += ACDIMMER::periodBuffer[i];
                }
                ACDIMMER::period = temp / PERIODBUFFER;
                ACDIMMER::readings = 0;
                return 1;
        }
        return 0;
}

void dimmer_set(int duty) {
        ACDIMMER::duty_save = duty;
        if (duty >= 100)
                ACDIMMER::duty_save = 100;
        if (duty <= 0)
                ACDIMMER::duty_save = 0;
        ACDIMMER::updateTime(ACDIMMER::duty_save);
}

void dimmer_on() {
        if (ACDIMMER::duty_old <= 20) {
                dimmer_move(100);
        } else {
                dimmer_move(ACDIMMER::duty_old);
        }
        ACDIMMER::status = 1;
}

void dimmer_off() {
        ACDIMMER::duty_old = ACDIMMER::duty_save;
        ACDIMMER::status = 0;
        dimmer_move(0);
}

void ACDIMMER::updateTime(int time) {
        static int temp;
        if (time <= 10 && temp >= 11) {
                temp = 10;
                detachInterrupt(digitalPinToInterrupt(ZC));
        } else {
                if (time >= 11 && temp <= 10) {
                        attachInterrupt(digitalPinToInterrupt(ZC), ACDIMMER::zeroCross, FALLING);
                }
                if (time >= 99) {
                        temp = 95;
                } else {
                        temp = time;
                }
        }

        ACDIMMER::tLow = ACDIMMER::period * (100 - temp) / 100;
}
/*
   void dimmer_move(int duty) {

        if (duty_save > duty) {
                direction = 0;
                steps = DIMMSPEED / (duty_save - duty);
        }
        if (duty_save < duty) {
                direction = 1;
                steps = DIMMSPEED / (duty - duty_save);
        }

        duty_goal = duty;
        tick.attach_ms(steps, tickHandler);
   }*/

void dimmer_move(int duty, int time_ms) {
        if (ACDIMMER::duty_save > duty) {
                ACDIMMER::direction = 0;
                ACDIMMER::steps = time_ms / (ACDIMMER::duty_save - duty);
        }
        if (ACDIMMER::duty_save < duty) {
                ACDIMMER::direction = 1;
                ACDIMMER::steps = time_ms / (duty - ACDIMMER::duty_save);
        }

        ACDIMMER::duty_goal = duty;
        ACDIMMER::tick.attach_ms(ACDIMMER::steps, ACDIMMER::tickHandler);
}

boolean dimmer_ismoving() {
        if (ACDIMMER::duty_goal == ACDIMMER::duty_save) {
                return false;
        } else {
                return true;
        }
}

int dimmer_status() {
        if (ACDIMMER::duty_save <= 10) {
                ACDIMMER::status = 0;
        }
        if (ACDIMMER::duty_save >= 11) {
                ACDIMMER::status = 1;
        }
        return ACDIMMER::status;
}

void dimmer_up() {
        dimmer_set(ACDIMMER::duty_save + 1);
}

void dimmer_down() {
        dimmer_set(ACDIMMER::duty_save - 1);
}

int dimmer_getDuty() {
        return ACDIMMER::duty_save;
}

void ACDIMMER::initPeriod() {
        ACDIMMER::thisZC = micros();
        ACDIMMER::readings++;
}

void ACDIMMER::zeroCross() {
        timer1_write(ACDIMMER::tLow * 5);
}
void ACDIMMER::tickHandler() {
        if (ACDIMMER::direction == 0) {
                dimmer_set(ACDIMMER::duty_save - 1);
        }
        if (ACDIMMER::direction == 1) {
                dimmer_set(ACDIMMER::duty_save + 1);
        }
        if (ACDIMMER::duty_goal == ACDIMMER::duty_save) tick.detach();
}

void ACDIMMER::handlerTimer() {
        if (ACDIMMER::flag_timer1) {
                digitalWrite(PWM, LOW);
                ACDIMMER::flag_timer1 = 0;
        } else {
                digitalWrite(PWM, HIGH);
                timer1_write(15 * 5);
                ACDIMMER::flag_timer1 = 1;
        }
}
