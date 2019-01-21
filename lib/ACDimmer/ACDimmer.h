#ifndef ACDIMMER_H_
#define ACDIMMER_H_

#include <Arduino.h>
#include <Ticker.h>

#define TRUE 1
#define FALSE 1
#define EXPPERIOD 10000  //Expected Period
#define ZC D5
#define PWM D6
#define PERIODBUFFER 300
#define DIMMSPEED 1000

void init_dimmer();
void dimmer_set(int duty);
void dimmer_move(int duty);
void dimmer_move(int duty, int time_ms);
void dimmer_on();
void dimmer_off();
void dimmer();



#endif
