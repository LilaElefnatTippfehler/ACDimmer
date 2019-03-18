#include "LEDstring.h"

namespace LEDSTRING
{
void tickHandlerLEDStr();
void wurstPWMHandler(uint8_t state);
void wurstPWM(int duty);

const uint32_t period = 5000; // * 200ns ^= 1 kHz
uint32 io_info[PWM_CHANNELS][3] = {{PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5, 5}};
int duty_save = 0;
int duty_goal = 0;
int duty_old = 0;
int direction = -1;
unsigned long steps = 0;
uint8_t status = 2;

Ticker tick;
Ticker wurstPWMever;
}


void init_ledStr() {
        pinMode(LS, OUTPUT);
        /*uint32 pwm_duty_init[PWM_CHANNELS] = {0};
           pwm_init(LEDSTRING::period, pwm_duty_init, PWM_CHANNELS, LEDSTRING::io_info);
           pwm_start();
           analogWrite(LS, 0);*/
        LEDSTRING::wurstPWMever.once_ms(10, LEDSTRING::wurstPWMHandler, (uint8_t) 1);
}


void ledStr_set(int duty) {
        LEDSTRING::duty_save = duty;
        if (duty >= 10)
                LEDSTRING::duty_save = 10;
        if (duty <= 0)
                LEDSTRING::duty_save = 0;

}

void ledStr_on() {
        if (LEDSTRING::duty_old <= 20) {
                ledStr_move(100);
        } else {
                ledStr_move(LEDSTRING::duty_old);
        }
        LEDSTRING::status = 1;
}

void ledStr_off() {
        LEDSTRING::duty_old = LEDSTRING::duty_save;
        LEDSTRING::status = 0;
        ledStr_move(0);
}

void ledStr_move(int duty, int time_ms) {
        if(duty <= 15) {    //flickering on low levels
                duty = 0;
        }
        duty = duty/10;
        if (LEDSTRING::duty_save > duty) {
                LEDSTRING::direction = 0;
                LEDSTRING::steps = time_ms / (LEDSTRING::duty_save - duty);
        }
        if (LEDSTRING::duty_save < duty) {
                LEDSTRING::direction = 1;
                LEDSTRING::steps = time_ms / (duty - LEDSTRING::duty_save);
        }
        Serial.println(duty);
        Serial.println(LEDSTRING::duty_save);
        Serial.println(LEDSTRING::steps);
        LEDSTRING::duty_goal = duty;
        LEDSTRING::tick.attach_ms(LEDSTRING::steps, LEDSTRING::tickHandlerLEDStr);
}

boolean ledStr_ismoving() {
        if (LEDSTRING::duty_goal == LEDSTRING::duty_save) {
                return false;
        } else {
                return true;
        }
}

int ledStr_status() {
        if (LEDSTRING::duty_save <= 10) {
                LEDSTRING::status = 0;
        }
        if (LEDSTRING::duty_save >= 11) {
                LEDSTRING::status = 1;
        }
        return LEDSTRING::status;
}

void ledStr_up() {
        ledStr_set(LEDSTRING::duty_save + 1);
}

void ledStr_down() {
        ledStr_set(LEDSTRING::duty_save - 1);
}

int ledStr_getDuty() {
        return LEDSTRING::duty_save;
}


void LEDSTRING::wurstPWM(int duty){
        if(duty > 10) duty = 10;
        if(duty < 0) duty = 0;

}

void LEDSTRING::wurstPWMHandler(uint8_t state){
        digitalWrite(LS, state);
        if(state == (uint8_t) 1) {
                LEDSTRING::wurstPWMever.once_ms(duty_save, wurstPWMHandler,(uint8_t) 0);
        }else{
                if(duty_save == 0) {    //flickering on low levels
                        LEDSTRING::wurstPWMever.once_ms(10, wurstPWMHandler,(uint8_t) 0);
                        return;
                }
                LEDSTRING::wurstPWMever.once_ms(10-duty_save, wurstPWMHandler,(uint8_t) 1);
        }

}

void LEDSTRING::tickHandlerLEDStr() {
        if (LEDSTRING::direction == 0) {
                ledStr_set(LEDSTRING::duty_save - 1);
        }
        if (LEDSTRING::direction == 1) {
                ledStr_set(LEDSTRING::duty_save + 1);
        }
        if (LEDSTRING::duty_goal == LEDSTRING::duty_save) {
                LEDSTRING::tick.detach();
        }
}
