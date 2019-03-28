#ifndef TOUCHAUTOMATCPP_HPP_
#define TOUCHAUTOMATCPP_HPP_

#include <Arduino.h>
#include <Ticker.h>

#define TOUCHTIME 1000

namespace TOUCHAUTOMAT
{

void touchDTISR_down();
void touchDTISR_up();
}

class touchAutomat {

private:
static bool instanceFlag;
static touchAutomat *single;
int status = 0;
uint8_t pin = 0;
long deltaTime = 0;
void touched(uint8_t state);
void (*function)(String,int,int );
touchAutomat(){

}
touchAutomat(touchAutomat const&);
touchAutomat operator=(touchAutomat const&);

protected:
void Tisr();

public:
static touchAutomat* instance();
static void touchISR();
~touchAutomat();
void init(void (*function)(String,int,int ),uint8_t touchPin);
void changeStatus(int status);
int getStatus();

};




#endif
