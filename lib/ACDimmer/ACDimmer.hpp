/*
   This class is far from perfect.
   it has singleton pattern, because the pin interrupt and the timer1 interrupt
   are hard to implement without that.
   Apart from that, this class is meant to be used with an esp8266 wich only has
   one timer, so you wont be able to controll two dimmers anyway.

   HOW TO USE THIS:
   ACDimmer *dimmer = ACDimmer::instance();
   dimmer->init(ZC,PWM);

   feel free tho change standart DIMMSPEED, PERIODBUFFER size for period mesurment
   and EXPPERIOD if your mains frequency is not 50Hz (Value is in us *1/2)


 */
#ifndef ACDIMMER_H_
#define ACDIMMER_H_

#include <Arduino.h>
#include <Ticker.h>

#define EXPPERIOD 10000 // Expected Period/2
#define PERIODBUFFER 300
#define DIMMSPEED 1000

class ACDimmer {

private:
static bool instanceFlag;
static ACDimmer *single;
uint8_t ZC;
uint8_t PWM;
int duty_save = 0;;
int duty_old = 0;
uint8_t status = 2;
long period = 0;
uint32_t tLow = 0;
int duty_goal = 0;
unsigned long thisZC = 0;
long readings = 1;
Ticker tick;
ACDimmer(){
}
int getPeriod();
void updateTime(int time);

protected:
void initPerISR();
void zcISR();
void TimerISR();
void tickISR(int direction);

public:
static ACDimmer* instance();
static void handlerTimer();
static void initPeriod();
static void zeroCross();
static void tickHandler(int direction);
void init(uint8_t ZC_t, uint8_t PWM_t);
~ACDimmer();
void set(int duty);
void move(int duty, int time_ms = DIMMSPEED);
void on();
void off();
bool getStatus();
int getDuty();
void up();
void down();
bool ismoving();

};


#endif
