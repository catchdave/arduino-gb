#include <Shifter.h>
#include <TimedAction.h>

// Shift Register config
#define NUM_REGISTERS 3 // how many registers are in the chain
int latchPin = 8;
int clockPin = 12;
int dataPin = 11;
Shifter shifter(dataPin, latchPin, clockPin, NUM_REGISTERS); 

// Booster config
#define BOOSTER_FIRST_PIN 0
#define BOOSTER_LAST_PIN 14
#define BOOSTER_DELAY_NORMAL 80
#define BOOSTER_DELAY_MIN 5
#define BOOSTER_DELAY_OVERLOAD 40
// Booster variables
int curBoosterPin = BOOSTER_FIRST_PIN;
int curBoosterDelay = BOOSTER_DELAY_NORMAL;
int boosterDir = 1;

// Cyclotron config
#define CYCLOTRON_FIRST_PIN 24
#define CYCLOTRON_LAST_PIN 27

// N-Filter Config
#define NFILTER_FIRST_PIN 15
#define NFILTER_LAST_PIN 23
#define NFILTER_DELAY_NORMAL 100
#define NFILTER_DELAY_OVERLOADED 1000
#define NFILTER_DELAY_FIRING 30
// N-Filter variables
int curNfilterPin = NFILTER_FIRST_PIN;

// General config
#define TIME_TO_OVERLOAD 6000UL
#define TIME_TO_COOLDOWN 4000UL
int switchOne = 2;

// State variables
int isFiring = false;
unsigned long overloadedStart = 0;
unsigned long firingStart = 0;
boolean stateOverloaded = false;

// Timed action
TimedAction normal_booster = TimedAction(BOOSTER_DELAY_NORMAL, boosterNormal);
TimedAction normal_nfilter = TimedAction(NFILTER_DELAY_NORMAL, nfilterNormal);
TimedAction overload_booster = TimedAction(BOOSTER_DELAY_NORMAL, boosterOverloaded);
TimedAction overload_nfilter = TimedAction(NFILTER_DELAY_OVERLOADED, nfilterOverloaded);

void setup()
{
  shifter.clear(); //set all pins on the shift register chain to LOW
  shifter.write();
  pinMode(switchOne, INPUT);
  
  overload_booster.disable();
  overload_nfilter.disable();
}

// Main loop
void loop()
{ 
  // Check firing
  isFiring = (digitalRead(switchOne) == HIGH);
  if (!isFiring) {
   firingStart = 0; 
  }
  else if (firingStart == 0) { // We just started firing
    firingStart = millis();   
  }
  
  // Overload start
  if (!stateOverloaded && isFiring && (millis() - firingStart) > TIME_TO_OVERLOAD) {
    stateOverloaded = true;
    overloadedStart = millis();
    
    resetAllLights();
    normal_booster.disable();
    normal_nfilter.disable();
    
    overload_booster.enable();
    overload_nfilter.enable();
  }

  // Overload endend
  if (stateOverloaded && !isFiring && (millis() - overloadedStart) > TIME_TO_COOLDOWN) {
    resetAllLights();
    stateOverloaded = false;
    
    normal_booster.enable();
    normal_nfilter.enable();
    
    overload_booster.disable();
    overload_nfilter.disable();    
  }
  
  // Exexucte events
  overload_booster.check();
  overload_nfilter.check();  
  normal_booster.check();
  normal_nfilter.check();
  
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

    // Increase or decrease speed on every new cycle
    if (isFiring) {
      curBoosterDelay -= 10;
      curBoosterDelay = max(curBoosterDelay, BOOSTER_DELAY_MIN);
    }
    else {
      curBoosterDelay += 10;
      curBoosterDelay = min(curBoosterDelay, BOOSTER_DELAY_NORMAL);
    }

    normal_booster.setInterval(curBoosterDelay);
  }

  shifter.setPin(curBoosterPin++, HIGH);
}

void boosterOverloaded()
{
  int atEnd = false;
  
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
  
  // Flash twice
 // boosterFlash(200);
  //boosterFlash(200);  
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
  int flashState = 0;
  
  for (int i = NFILTER_FIRST_PIN; i <= NFILTER_LAST_PIN; i++) {
     shifter.setPin(i, ((flashState % 2 == 0) && flashState < 8) ? HIGH : LOW);
  }
  
  if (flashState++ > 8) {
    flashState = 0; 
  }
}

void nfilterClear()
{
  for (int i = NFILTER_FIRST_PIN; i <= NFILTER_LAST_PIN; i++) {
     shifter.setPin(i, LOW);
  }
}

/*
void loop()
{ 
  isFiring = (digitalRead(switchOne) == HIGH);

  // If we've reached our min, then start overload sequence
  if (overloaded) {
     overloadedBooster();

     // End overloaded state
     isFiring = (digitalRead(switchOne) == HIGH);
     if (isFiring) {
       overloadedStart = millis(); // still firing
     }
     else if (!isFiring && (millis() - overloadedStart) > OVERLOADED_TIMER) {
        overloaded = false;
        curBoosterDelay += 10; // ensure we don't immediately trip overload again
     }
  }
  else {
     normalBooster();
     
     // Should we start overloaded state?
    if (!overloaded && curBoosterDelay <= BOOSTER_DELAY_MIN) {
      overloaded = true;
      overloadedStart = millis();
    }
  }
}



void boosterFlash(int flashDelay)
{
  shifter.setAll(HIGH);
  shifter.write();
  delay(flashDelay);
  shifter.clear();
  shifter.write();
  delay(flashDelay); 
}
*/

