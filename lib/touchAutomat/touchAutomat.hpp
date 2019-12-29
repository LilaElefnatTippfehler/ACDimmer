/*
   This class is far from perfect.
   it has singleton pattern, because the pin interrupt is hard to implement
   without that.

   HOW TO USE:
   touchAutomat *ta = touchAutomat::instance();
   ta->init(function_to_activate,TOUCH_PIN);

   As this is far from perfect, function_to_activate should look like this:
   void function_to_activate(String command, int value, int time);
   its intendet to use with ACDimmer and LEDString.

   Feel free to change TOUCHTIME, it defines the time in ms for long touches
 */

#ifndef TOUCHAUTOMAT_HPP_
#define TOUCHAUTOMAT_HPP_

#include <Arduino.h>
#include <Ticker.h>

#define TOUCHTIME 500

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
