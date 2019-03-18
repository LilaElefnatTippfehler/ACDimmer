#ifndef LEDSTRING_H_
#define LEDSTRING_H_

#include <Arduino.h>
#include <Ticker.h>

#define DIMMSPEED 1000
#define LS D1
#define PWM_CHANNELS 1

void init_ledStr();
void init_ledStr_stead();
void ledStr_set(int duty);
void ledStr_move(int duty, int time_ms = DIMMSPEED);
void ledStr_on();
void ledStr_off();
int ledStr_status();
int ledStr_getDuty();
void ledStr_up();
void ledStr_down();
boolean ledStr_ismoving();

#endif
