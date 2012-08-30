#include <Shifter.h>
#include <TimedAction.h>
#include "WaveUtil.h"
#include "WaveHC.h"

#define uint unsigned int
#define ulong unsigned long

// Pin Definitions
int latchPin = 8;
int clockPin = 7; // used to be 12
int dataPin = 9; // used to be 11
int triggerSwitch = A5;

// General config
const ulong TIME_TO_COOLDOWN = 5000UL;
const int MAX_LEVELS = 20;
int curLevel = 0;

// State variables
boolean isFiring = false;
boolean stateOverloaded = false;
ulong overloadedStart = 0;
ulong firingStart = 0;
boolean initialised = false;

// Shift Register config
const int NUM_REGISTERS = 7; // how many registers are in the chain
Shifter shifter(dataPin, latchPin, clockPin, NUM_REGISTERS);

// Booster config
const int BOOSTER_FIRST_PIN = 0;
const int BOOSTER_LAST_PIN = 14;
const int BOOSTER_LEVELS[MAX_LEVELS+1] = {10,10,10,15,15,15,20,20,20,20,20,20,30,30,30,30,40,40,40,40,80};
const int BOOSTER_DELAY_INIT = BOOSTER_LEVELS[19];
const int BOOSTER_DELAY_OVERLOADED = 80;
// Booster variables
int curBoosterPin = BOOSTER_FIRST_PIN;
int curBoosterDelay = BOOSTER_DELAY_INIT;
int boosterDir = 1;

// N-Filter Config
const int NFILTER_FIRST_PIN = 15;
const int NFILTER_LAST_PIN = 23;
const int NFILTER_DELAY_NORMAL = 100;
const int NFILTER_DELAY_OVERLOADED = 250;
const int NFILTER_DELAY_FIRING = 30;
// N-Filter variables
int curNfilterPin = NFILTER_FIRST_PIN;
int nFilterFlashState = 0;

// Cyclotron config
const int CYCLOTRON_FIRST_PIN = 24;
const int CYCLOTRON_LAST_PIN = 27;
const int CYCLOTRON_DELAY_NORMAL = 350;
// Cyclotron variables
int curCyclotronPin = CYCLOTRON_FIRST_PIN;

// Gun Bargraph
const int BARGRAPH_FIRST_PIN = 28;
const int BARGRAPH_LAST_PIN = 47;
const int BARGRAPH_MIDDLE_PIN = 38;
const int BARGRAPH_DELAY_NORMAL_UP = 420;
const int BARGRAPH_DELAY_NORMAL_DOWN = 350;
const int BARGRAPH_DELAY_OVERLOADED = 70;
const int BARGRAPH_DELAY_CYCLE = 70;
// Bargraph variables
int primaryBargraphPin = BARGRAPH_FIRST_PIN;
int secondaryBargraphPin = BARGRAPH_MIDDLE_PIN;
int bargraphCyclePin = BARGRAPH_LAST_PIN;
boolean bargraphUp = true;
boolean bargraphWaiting = false;
int bargraphDir = 1;

// Timed action, normal
TimedAction normal_booster = TimedAction(BOOSTER_DELAY_INIT, boosterNormal);
TimedAction normal_nfilter = TimedAction(NFILTER_DELAY_NORMAL, nfilterNormal);
TimedAction normal_cyclotron = TimedAction(CYCLOTRON_DELAY_NORMAL, cyclotronNormal);
TimedAction normal_bargraph = TimedAction(BARGRAPH_DELAY_NORMAL_DOWN, bargraphNormal);
TimedAction normal_bargraph_cycle = TimedAction(BARGRAPH_DELAY_CYCLE, bargraphCycle);

// Timed action, overload
TimedAction overload_booster = TimedAction(BOOSTER_DELAY_OVERLOADED, boosterOverloaded);
TimedAction overload_nfilter = TimedAction(NFILTER_DELAY_OVERLOADED, nfilterOverloaded);
TimedAction overload_bargraph = TimedAction(BARGRAPH_DELAY_OVERLOADED, bargraphOverloaded);

WaveHC wave(Serial);  // This is the only wave (audio) object, since we will only play one at a time

void setup()
{
  shifter.clear(); //set all pins on the shift register chain to LOW
  shifter.write();
  pinMode(triggerSwitch, INPUT);

  overload_booster.disable();
  overload_nfilter.disable();
  overload_bargraph.disable();

  normal_cyclotron.disable(); // enabled when initialised
  normal_bargraph_cycle.disable(); // enbled when bargraph full

  //wave.setup();
  Serial.begin(9600);
}

// Main loop
void loop()
{
  /*
  if (!wave.isplaying) {
    wave.playfile("theme1.wav");
  }
  return;
  */


  // Check firing
  isFiring = initialised && (digitalRead(triggerSwitch) == HIGH);
  if (!isFiring) {
   firingStart = 0;
  }
  else if (firingStart == 0) { // We just started firing
    firingStart = millis();
  }

  // Overload start
  if (!stateOverloaded && isFiring && curLevel <= 0) {
    stateOverloaded = true;
    resetAllLights();

    normal_booster.disable();
    normal_nfilter.disable();
    normal_bargraph.disable();
    normal_bargraph_cycle.disable();

    overload_booster.enable();
    overload_nfilter.enable();
    overload_bargraph.enable();
  }

  // Update overload start time, if still firing
  if (stateOverloaded && isFiring) {
    overloadedStart = millis();
  }

  // Overload endend
  if (stateOverloaded && !isFiring && (millis() - overloadedStart) > TIME_TO_COOLDOWN) {
    stateOverloaded = false;
    resetAllLights();

    normal_booster.enable();
    normal_nfilter.enable();
    normal_bargraph.enable();

    overload_booster.disable();
    overload_nfilter.disable();
    overload_bargraph.disable();
  }

  // Execute events
  overload_booster.check();
  overload_nfilter.check();
  overload_bargraph.check();
  normal_booster.check();
  normal_nfilter.check();
  normal_cyclotron.check();
  normal_bargraph.check();
  normal_bargraph_cycle.check();

  // Send LED changes
  if (shifter.isUpdateNeeded()) {
    shifter.write();
  }
}

void resetAllLights()
{
  curBoosterPin = BOOSTER_FIRST_PIN;
  boosterDir = 1;
  boosterClear();

  curNfilterPin = NFILTER_FIRST_PIN;
  nfilterClear();

  bargraphReset();
}

/*********************
 * Booster Functions *
 *********************/
void boosterNormal()
{
  // End of cycle, start again
  if (curBoosterPin > BOOSTER_LAST_PIN) {
    curBoosterPin = BOOSTER_FIRST_PIN;
    boosterClear();
  }

  // Speed is dependant on current level
  if (initialised) {
    normal_booster.setInterval(BOOSTER_LEVELS[curLevel]);
  }

  shifter.setPin(curBoosterPin++, HIGH);
}

void boosterOverloaded()
{
  int atEnd;

  if (curBoosterPin >= BOOSTER_LAST_PIN) {
    boosterDir = -1;
    atEnd = true;
  }
  else if (curBoosterPin <= BOOSTER_FIRST_PIN) {
     boosterDir = 1;
     atEnd = true;
  }
  else {
    atEnd = false;
  }

  boosterClear();
  shifter.setPin(curBoosterPin, HIGH);
  curBoosterPin = curBoosterPin + boosterDir;
  if (!atEnd) {
    shifter.setPin(curBoosterPin, HIGH);
  }
}

void boosterClear()
{
  for (int i = BOOSTER_FIRST_PIN; i <= BOOSTER_LAST_PIN; i++) {
     shifter.setPin(i, LOW);
  }
}

/*********************
 * N-Filter Functions *
 *********************/
void nfilterNormal()
{
  shifter.setPin(curNfilterPin, LOW);

  //
  if (isFiring) {
    normal_nfilter.setInterval(NFILTER_DELAY_FIRING);

    curNfilterPin--;
    if (curNfilterPin < NFILTER_FIRST_PIN) {
      curNfilterPin = NFILTER_LAST_PIN;
    }
  }
  else {
    normal_nfilter.setInterval(NFILTER_DELAY_NORMAL);

    curNfilterPin++;
    if (curNfilterPin > NFILTER_LAST_PIN) {
      curNfilterPin = NFILTER_FIRST_PIN;
    }
  }

  shifter.setPin(curNfilterPin, HIGH);
}

void nfilterOverloaded()
{
  for (int i = NFILTER_FIRST_PIN; i <= NFILTER_LAST_PIN; i++) {
     shifter.setPin(i, (nFilterFlashState == 0 || nFilterFlashState == 2) ? HIGH : LOW);
  }

  if (nFilterFlashState++ > 6) {
    nFilterFlashState = 0;
  }
}

void nfilterClear()
{
  for (int i = NFILTER_FIRST_PIN; i <= NFILTER_LAST_PIN; i++) {
     shifter.setPin(i, LOW);
  }
}

/*********************
 * Cyclotron Functions *
 *********************/
void cyclotronNormal()
{
  shifter.setPin(curCyclotronPin++, LOW);

  if (curCyclotronPin > CYCLOTRON_LAST_PIN) {
    curCyclotronPin = CYCLOTRON_FIRST_PIN;
  }
  shifter.setPin(curCyclotronPin, HIGH);
}

/*********************
 * Bargraph Functions *
 *********************/

/**
 * Responsible for maintaining the curLevel of overload.
 * When curLevel >= MAX_LEVELS
 */
void bargraphNormal()
{
  if (isFiring) {

    normal_bargraph_cycle.disable();
    bargraphCyclePin = 0;

    if (--curLevel < 0) {
       curLevel = 0;
    }
    normal_bargraph.setInterval(BARGRAPH_DELAY_NORMAL_DOWN);
  }
  else if(!stateOverloaded) {
    if (curLevel == MAX_LEVELS - 1) {
       curLevel = MAX_LEVELS;
       normal_bargraph_cycle.enable();
       normal_cyclotron.enable();
       initialised = true;
    }
    else if (curLevel < MAX_LEVELS) {
       curLevel++;
    }
    normal_bargraph.setInterval(BARGRAPH_DELAY_NORMAL_UP);
  }

  for (int i = BARGRAPH_FIRST_PIN; i <= BARGRAPH_LAST_PIN; i++) {
    shifter.setPin(i, ((i < BARGRAPH_FIRST_PIN + curLevel) && i != bargraphCyclePin) ? HIGH : LOW);
  }
}

void bargraphCycle()
{
  int revertPin;

  if (--bargraphCyclePin < BARGRAPH_FIRST_PIN) {
    bargraphCyclePin = BARGRAPH_LAST_PIN;
    revertPin = BARGRAPH_FIRST_PIN;
  }
  else {
    revertPin = bargraphCyclePin + 1;
  }

  if (revertPin < (BARGRAPH_FIRST_PIN + curLevel)) {
     shifter.setPin(revertPin,  HIGH);
  }

  shifter.setPin(bargraphCyclePin,  LOW);
}

void bargraphOverloaded()
{
  if (primaryBargraphPin < BARGRAPH_FIRST_PIN) {
    bargraphUp = false;

  }
  else if (primaryBargraphPin >= BARGRAPH_MIDDLE_PIN - 1) {
    bargraphUp = true;
  }

  if (bargraphUp) {
    shifter.setPin(primaryBargraphPin--, HIGH);
    shifter.setPin(secondaryBargraphPin++, HIGH);
  }
  else {
    shifter.setPin(primaryBargraphPin++, LOW);
    shifter.setPin(secondaryBargraphPin--, LOW);
  }
}

void bargraphReset()
{
  bargraphUp = true;
  bargraphDir = 1;
  if (stateOverloaded) {
    primaryBargraphPin = BARGRAPH_MIDDLE_PIN - 1;
  }
  else {
    primaryBargraphPin = BARGRAPH_FIRST_PIN;
  }
  secondaryBargraphPin = BARGRAPH_MIDDLE_PIN;

  for (int i = BARGRAPH_FIRST_PIN; i <= BARGRAPH_LAST_PIN; i++) {
     shifter.setPin(i, LOW);
  }
}
