#include "ACDimmer.hpp"
#include <Ticker.h>
#include <Arduino.h>

bool ACDimmer::instanceFlag = false;
ACDimmer* ACDimmer::single = NULL;
ACDimmer* ACDimmer::instance(){
        if(!instanceFlag) {
                single = new ACDimmer();
                instanceFlag = true;
                return single;
        }
        else
        {
                return single;
        }
}
ACDimmer::~ACDimmer(){
        instanceFlag = false;
}

void ACDimmer::init(uint8_t ZC_t, uint8_t PWM_t){
        ZC = ZC_t; PWM = PWM_t;
        pinMode(ZC, INPUT_PULLUP);
        pinMode(PWM, OUTPUT);
        if(SERIAL) Serial.println("after pinMode");
        timer1_isr_init();
        if(SERIAL) Serial.println("befor timer1 interrupt");
        timer1_attachInterrupt(this->handlerTimer);
        if(SERIAL) Serial.println("befor timer1 enable");
        timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
        if(SERIAL) Serial.println("after all timer1");
        attachInterrupt(digitalPinToInterrupt(ZC), this->initPeriod, FALLING);
        if(SERIAL) Serial.println("after initPeriod");
        while (!this->getPeriod());
        detachInterrupt(digitalPinToInterrupt(ZC));
        attachInterrupt(digitalPinToInterrupt(ZC), this->zeroCross, FALLING);
        if(SERIAL) Serial.println("after zeroCross");

        this->set(50);
        this->set(1);
}

int ACDimmer::getPeriod(){
        int temp = 0;
        static unsigned long lastZC = 0;
        static long periodBuffer[PERIODBUFFER];
        ESP.wdtFeed();
        if (lastZC == 0) {
                lastZC = thisZC;
                return 0;
        }
        if (lastZC >= thisZC) {
                period = EXPPERIOD;
                return 0;
        }
        if ((thisZC - lastZC) >= (EXPPERIOD + 400) || (thisZC - lastZC) <= (EXPPERIOD - 400)) {
                periodBuffer[readings % PERIODBUFFER] = periodBuffer[readings % PERIODBUFFER - 1];
        } else {
                periodBuffer[readings % PERIODBUFFER] = thisZC - lastZC;
        }
        lastZC = thisZC;
        if (!(readings % PERIODBUFFER)) {
                for (int i = 0; i < PERIODBUFFER; i++) {
                        temp += periodBuffer[i];
                }
                period = temp / PERIODBUFFER;
                readings = 0;
                return 1;
        }
        return 0;
}

void ACDimmer::set(int duty){
        duty_save = duty;
        if (duty >= 100)
                duty_save = 100;
        if (duty <= 0)
                duty_save = 0;
        this->updateTime(duty_save);
}

void ACDimmer::on() {
        if (duty_old <= 20) {
                this->move(100);
        } else {
                this->move(duty_old);
        }
        status = 1;
}

void ACDimmer::off() {
        duty_old = duty_save;
        status = 0;
        this->move(0);
}

void ACDimmer::updateTime(int time) {
        static int temp;
        if (time <= 10 && temp >= 11) {
                temp = 10;
                detachInterrupt(digitalPinToInterrupt(ZC));
        } else {
                if (time >= 11 && temp <= 10) {
                        attachInterrupt(digitalPinToInterrupt(ZC), this->zeroCross, FALLING);
                }
                if (time >= 99) {
                        temp = 95;
                } else {
                        temp = time;
                }
        }

        tLow = period * (100 - temp) / 100;
}

void ACDimmer::move(int duty, int time_ms) {
        int direction = 0;
        unsigned long steps = 0;
        if (duty_save > duty) {
                direction = 0;
                steps = time_ms / (duty_save - duty);
        }
        if (duty_save < duty) {
                direction = 1;
                steps = time_ms / (duty - duty_save);
        }

        duty_goal = duty;
        tick.attach_ms(steps, tickHandler, direction);
}

bool ACDimmer::ismoving() {
        if (duty_goal == duty_save) {
                return false;
        } else {
                return true;
        }
}

bool ACDimmer::getStatus() {
        if (duty_save <= 10) {
                status = false;
        }
        if (duty_save >= 11) {
                status = true;
        }
        return status;
}

void ACDimmer::up() {
        this->set(duty_save + 1);
}

void ACDimmer::down() {
        this->set(duty_save - 1);
}

int ACDimmer::getDuty() {
        return duty_save;
}

void ICACHE_RAM_ATTR ACDimmer::initPeriod() {
        ACDimmer::instance()->initPerISR();
}

void ACDimmer::initPerISR(){
        thisZC = micros();
        readings++;
}

void ICACHE_RAM_ATTR ACDimmer::zeroCross(){
        ACDimmer::instance()->zcISR();
}

void ACDimmer::zcISR(){
        timer1_write(this->tLow * 5);
}

void ICACHE_RAM_ATTR ACDimmer::handlerTimer(){
        ACDimmer::instance()->TimerISR();
}

void ACDimmer::TimerISR(){
        static bool flag_timer = false;
        if(flag_timer) {
                digitalWrite(PWM, LOW);
                flag_timer = false;
        }else{
                digitalWrite(PWM, HIGH);
                timer1_write(15*5);
                flag_timer = true;
        }
}

void ACDimmer::tickHandler(int direction){
        ACDimmer::instance()->tickISR(direction);
}

void ACDimmer::tickISR(int direction){
        if(direction == 0) this->set(this->duty_save - 1);
        if(direction == 1) this->set(this->duty_save + 1);
        if(!this->ismoving()) this->tick.detach();
}
