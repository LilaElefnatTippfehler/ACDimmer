
#include <Arduino.h>
#include <Ticker.h>
#include "ACDimmer.h"

#define TOUCHTIME 1000

void touchISR();
void touchDTISR_up();
void touchDTISR_down();
void touchAutomat();
