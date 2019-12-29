
#include "LEDString.hpp"


LEDString::LEDString(uint8_t LEDPin){
        Pin = LEDPin;
}

void LEDString::init(){
        pinMode(Pin,OUTPUT);
        wurstPWMever.once_ms(10, LEDSTRING::wurstPWMHandler, this);
}

void LEDString::set(int duty){
        duty_save = duty;
        if(duty >= 10) duty_save = 10;
        if(duty <= 0) duty_save = 0;
}

void LEDString::on(){
        if(duty_old <= 20) {
                LEDString::move(100);
        }else{
                LEDString::move(duty_old);
        }
        LEDString::status = 1;
}

void LEDString::off(){
        duty_old = duty_save;
        status = 0;
        LEDString::move(0);
}

void LEDString::move(int duty, int time_ms){
        if(duty <= 15) { //flickering on low levels
                duty = 0;
        }
        duty = duty/10;
        if (duty_save > duty) {
                direction = 0;
                steps = time_ms / (duty_save - duty);
        }
        if (duty_save < duty) {
                direction = 1;
                steps = time_ms / (duty - duty_save);
        }
        duty_goal = duty;
        tick.attach_ms(steps, LEDSTRING::tickHandlerLEDStr,this);
}

bool LEDString::ismoving(){
        if(duty_goal == duty_save) {
                return false;
        } else {
                return true;
        }
}

bool LEDString::getStatus(){
        if (duty_save == 0) {
                status = false;
        }
        if (duty_save >= 1) {
                status = true;
        }
        return status;
}

void LEDString::up(){
        set(duty_save+1);
}

void LEDString::down(){
        set(duty_save-1);
}

int LEDString::getDuty(){
        return duty_save * 10;
}

int LEDString::getDirection(){
        return direction;
}

void LEDSTRING::tickHandlerLEDStr(LEDString *obj){
        if(obj->getDirection() == 0) {
                obj->down();
        }
        if(obj->getDirection() == 1) {
                obj->up();
        }
        if(!obj->ismoving()) {
                obj->tick.detach();
        }
}

void LEDSTRING::wurstPWMHandler(LEDString *obj){
        digitalWrite(obj->Pin, obj->state);
        if(obj->state == (uint8_t) 1) {
                obj->state = 0;
                obj->wurstPWMever.once_ms(obj->getDuty()/10, LEDSTRING::wurstPWMHandler,obj);
        }else{
                if(obj->getDuty() == 0) {
                        obj->state = 0;
                        obj->wurstPWMever.once_ms(10, LEDSTRING::wurstPWMHandler, obj);
                        return;
                }
                obj->state = 1;
                obj->wurstPWMever.once_ms(10-obj->getDuty()/10, LEDSTRING::wurstPWMHandler,obj);
                return;
        }
}
