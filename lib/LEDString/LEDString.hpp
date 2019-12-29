/*
   This class is far from perfect.
   It uses global ISR for the ticker.

   It implements an pseudo PWM through the ticker function.
   Solution is only in ms and to prevent flickering it only dimms the light
   in 10 steps.

   HOW TO USE:
   LEDString lamps(LAMPS_PIN);
   lamps.init();

   Feel free to change the standart DIMMSPEED.

 */

#ifndef LEDSTRING_HPP_
#define LEDSTRING_HPP_

#include <Ticker.h>
#include <Arduino.h>

#define DIMMSPEED 1000



class LEDString {

int duty_save = 0;
int duty_goal = 0;
int duty_old = 0;
int direction = -1;
unsigned long steps = 0;
uint8_t status = 2;

public:
Ticker tick;
Ticker wurstPWMever;
uint8_t Pin = 0;
uint8_t state = 0;
LEDString(uint8_t LEDPin);
void init();
void set(int duty);
void move(int duty, int time_ms = DIMMSPEED);
void on();
void off();
bool getStatus();
int getDuty();
void up();
void down();
bool ismoving();
int getDirection();
};

namespace LEDSTRING
{
void tickHandlerLEDStr(LEDString *obj);
void wurstPWMHandler(LEDString *obj);
void wurstPWM(int duty);

}

#endif
