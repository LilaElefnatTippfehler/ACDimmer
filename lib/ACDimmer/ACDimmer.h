#ifndef ACDIMMER_H_
#define ACDIMMER_H_

#include <Arduino.h>
#include <Ticker.h>

#define EXPPERIOD 10000 // Expected Period
#define ZC D5
#define PWM D6
#define PERIODBUFFER 300
#define DIMMSPEED 1000
#define LS D2

void init_dimmer();
void dimmer_set(int duty);
void dimmer_move(int duty);
void dimmer_move(int duty, int time_ms);
void dimmer_on();
void dimmer_off();
int dimmer_status();
int dimmer_getDuty();
void dimmer_up();
void dimmer_down();
boolean dimmer_ismoving();

#endif
