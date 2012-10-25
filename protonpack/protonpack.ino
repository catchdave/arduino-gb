#include <Shifter.h>
#include <TimedAction.h>
#include "WaveUtil.h"
#include "WaveHC.h"
//#include <GBModule.h>

#define uint unsigned int
#define ulong unsigned long

// Pin Definitions
int CLOCK_PIN = 7; // used to be 12 (orange)
int LATCH_PIN = 8; // (green)
int DATA_PIN = 9; // used to be 11 (blue)
int TRIGGER_SWITCH = A5;

// General config
const ulong TIME_TO_COOLDOWN = 6500UL; // Compare to BARGRAPH_DELAY_NORMAL_UP and 
int curLevel = 3; // Start partway to match startup sound to time taken to initialise

// State variables
boolean isFiring = false;
boolean stateOverloaded = false;
ulong overloadedStart = 0;
ulong firingStart = 0;
boolean initialised = false;
boolean firingSound = true; // flag to flip between 2 types of firing sound

// Shift Register config
const int NUM_REGISTERS = 6; // how many registers are in the chain
Shifter shifter(DATA_PIN, LATCH_PIN, CLOCK_PIN, NUM_REGISTERS);

// Theme / background music config
int THEME_SWITCH = A4;
bool themePlaying = false;
int curTheme = 0;

// Misc gun config
const int SLO_BLO_PIN = 43;
const int GUNTOP_PIN = 42;
const int SLO_BLO_DELAY = 800;
const int GUNTOP_DELAY = 400;
boolean slobloState = false;
int guntopState = 0;

// Laser Config
const int LASER_PIN = A0;
const int LASER_OPTIONS = 7; // number of duration/delay options
const int LASER_DURATIONS[LASER_OPTIONS] = {400, 220, 200, 100, 80, 40, 20};
const int LASER_DELAYS[LASER_OPTIONS] = {140, 80, 60, 50, 40, 20, 10};
boolean laserState = false; // is laser on or off?

// Cyclotron config
const int CYCLOTRON_PINS[4][3] = {
  {19, 27, 28},
  {20, 23, 24},
  {21, 25, 26},
  {22, 29, 30}
};
const int CYCLOTRON_FIRST_PIN = 19;
const int CYCLOTRON_LAST_PIN = 30;
const int CYCLOTRON_COUNT = 4; // 4 holes in the cyclotron
const int CYCLOTRON_DELAY_NORMAL = 450;
const int CYCLOTRON_DELAY_OVERLOADED = 250;
const int CYCLOTRON_DELAY_FIRING = 80;
// Cyclotron config
int curCyclotronGroup = 0;
int cyclotronFlashState = 0;

// Gun Bargraph
const int BARGRAPH_FIRST_PIN = 32;
const int BARGRAPH_LAST_PIN = 41;
const int BARGRAPH_MIDDLE_PIN = 37;
const int BARGRAPH_DELAY_NORMAL_UP = 700; // This controls the time to recharge
const int BARGRAPH_DELAY_NORMAL_DOWN = 600; // This controls how long you can fire for (multiply by MAX_LEVELS to get firing time)
const int BARGRAPH_DELAY_OVERLOADED = 90;
const int BARGRAPH_DELAY_CYCLE = 100;
const int MAX_LEVELS = 1 + BARGRAPH_LAST_PIN - BARGRAPH_FIRST_PIN;
// Bargraph variables
int primaryBargraphPin = BARGRAPH_FIRST_PIN;
int secondaryBargraphPin = BARGRAPH_LAST_PIN;
int bargraphCyclePin = BARGRAPH_LAST_PIN;
boolean bargraphUp = true;

// Booster config
const int BOOSTER_FIRST_PIN = 0;
const int BOOSTER_LAST_PIN = 13;
const int BOOSTER_LEVELS[MAX_LEVELS+1] = {10,10,15,20,20,30,40,40,80,90,120};
const int BOOSTER_DELAY_INIT = BOOSTER_LEVELS[MAX_LEVELS-1];
const int BOOSTER_DELAY_OVERLOADED = 80;
// Booster variables
int curBoosterPin = BOOSTER_FIRST_PIN;
int curBoosterDelay = BOOSTER_DELAY_INIT;
int boosterDir = 1;

// Timed action, normal
TimedAction normal_booster = TimedAction(BOOSTER_DELAY_INIT, boosterNormal);
TimedAction normal_cyclotron = TimedAction(CYCLOTRON_DELAY_NORMAL, cyclotronNormal);
TimedAction normal_bargraph = TimedAction(BARGRAPH_DELAY_NORMAL_DOWN, bargraphNormal);
TimedAction normal_bargraph_cycle = TimedAction(BARGRAPH_DELAY_CYCLE, bargraphCycle);
TimedAction normal_sloblo = TimedAction(SLO_BLO_DELAY, slobloNormal);
TimedAction normal_guntop = TimedAction(GUNTOP_DELAY, guntopNormal);

// Timed actions for firing only
TimedAction firing_laser = TimedAction(LASER_DELAYS[0], laserFiring);

// Timed action, overload
TimedAction overload_booster = TimedAction(BOOSTER_DELAY_OVERLOADED, boosterOverloaded);
TimedAction overload_cyclotron = TimedAction(CYCLOTRON_DELAY_OVERLOADED, cyclotronOverloaded);
TimedAction overload_bargraph = TimedAction(BARGRAPH_DELAY_OVERLOADED, bargraphOverloaded);

WaveHC wave(Serial);  // This is the only wave (audio) object, since we will only play one at a time

void setup()
{
  pinMode(TRIGGER_SWITCH, INPUT);
  pinMode(THEME_SWITCH, INPUT);
  
  pinMode(LASER_PIN, OUTPUT);
  pinMode(SLO_BLO_PIN, OUTPUT);
  pinMode(GUNTOP_PIN, OUTPUT);

  overload_cyclotron.disable();
  overload_booster.disable();
  overload_bargraph.disable();
  firing_laser.disable();
  normal_cyclotron.disable(); // enabled when initialised
  normal_bargraph_cycle.disable(); // enbled when bargraph full

  shifter.clear(); // set all pins on the shift register chain to LOW
  shifter.write();
  
  wave.setup();
  wave.playfile("p_start.wav"); // plays only once on first load
}

// Main loop
void loop()
{
  bool themePlay = digitalRead(THEME_SWITCH) == LOW;
  bool anythingPlaying = wave.isplaying;
  bool canChangeSoundState = !wave.isplaying || themePlaying;

  // Check firing
  isFiring = initialised && (digitalRead(TRIGGER_SWITCH) == LOW);
  if (!isFiring) {
    if (!stateOverloaded && firingStart > 0) {
      wave.playfile("p_stop.wav");
    }
    firingStart = 0;
    firing_laser.disable();
  }
  else if (firingStart == 0 && !stateOverloaded) { // We just started firing
    firingStart = millis();
    firing_laser.enable();
    if (firingSound) {
      wave.playfile("fire.wav");
    } else {
      wave.playfile("fire2.wav");
    }
    themePlaying = false;
    firingSound = !firingSound;
  }

  if (!anythingPlaying && themePlay) {
    String filename = "theme";
    filename += curTheme;
    filename += ".wav";
    char charBuf[filename.length()+1];
    filename.toCharArray(charBuf, filename.length()+1);
    wave.playfile(charBuf);

    themePlaying = true;
  }
  if (themePlaying && !themePlay) {
     wave.stop();
     themePlaying = false;

     // Switch themes
     if (++curTheme > 3) {
       curTheme = 0;
     }
  }

  // Overload start
  if (!stateOverloaded && isFiring && curLevel <= 0) {
    wave.playfile("p_stop.wav");
    themePlaying = false;

    stateOverloaded = true;
    resetAllLights();

    firing_laser.disable();
    normal_booster.disable();
    normal_cyclotron.disable();
    normal_bargraph.disable();
    normal_bargraph_cycle.disable();

    overload_booster.enable();
    overload_cyclotron.enable();
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
    normal_cyclotron.enable();
    normal_bargraph.enable();

    overload_booster.disable();
    overload_cyclotron.disable();
    overload_bargraph.disable();
  }

  // Execute events
  overload_booster.check();
  overload_cyclotron.check();
  overload_bargraph.check();

  normal_booster.check();
  normal_cyclotron.check();
  normal_bargraph.check();
  normal_bargraph_cycle.check();
  normal_sloblo.check();
  normal_guntop.check();
  firing_laser.check();

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

  curCyclotronGroup = 0;
  cyclotronClear();

  bargraphReset();
  
  digitalWrite(LASER_PIN,  LOW);
  shifter.setPin(SLO_BLO_PIN, LOW);
  shifter.setPin(GUNTOP_PIN, LOW);
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
 * Cyclotron Functions *
 *********************/
void cyclotronNormal()
{
  cyclotronSetPinGroup(curCyclotronGroup, LOW);
  normal_cyclotron.setInterval(isFiring ? CYCLOTRON_DELAY_FIRING : CYCLOTRON_DELAY_NORMAL);
    
  if (isFiring) {
    curCyclotronGroup--;
    if (curCyclotronGroup < 0) {
      curCyclotronGroup = CYCLOTRON_COUNT - 1;
    }
  }
  else {
    curCyclotronGroup++;
    if (curCyclotronGroup >= CYCLOTRON_COUNT) {
      curCyclotronGroup = 0;
    }
  }

 cyclotronSetPinGroup(curCyclotronGroup, HIGH);
}

void cyclotronSetPinGroup(int group, int state)
{
  for (int i = 0; i < 3; i++) {
   shifter.setPin(CYCLOTRON_PINS[curCyclotronGroup][i], state);
  }
}

void cyclotronOverloaded()
{
  for (int i = CYCLOTRON_FIRST_PIN; i <= CYCLOTRON_LAST_PIN; i++) {
     shifter.setPin(i, (cyclotronFlashState == 0 || cyclotronFlashState == 2) ? HIGH : LOW);
  }

  if (cyclotronFlashState++ > 6) {
    cyclotronFlashState = 0;
  }
}

void cyclotronClear()
{
  for (int i = CYCLOTRON_FIRST_PIN; i <= CYCLOTRON_LAST_PIN; i++) {
     shifter.setPin(i, LOW);
  }
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
  int inversePin; // The PIN in the graph that is off (and cycles through)
  
  if (--bargraphCyclePin < BARGRAPH_FIRST_PIN) {
    bargraphCyclePin = BARGRAPH_LAST_PIN;
    inversePin = BARGRAPH_FIRST_PIN;
  }
  else {
    inversePin = bargraphCyclePin + 1;
  }

  if (inversePin < (BARGRAPH_FIRST_PIN + curLevel)) {
     shifter.setPin(inversePin,  HIGH);
  }

  shifter.setPin(bargraphCyclePin,  LOW);
}

void bargraphOverloaded()
{ 
  if (primaryBargraphPin <= BARGRAPH_FIRST_PIN) {
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
  primaryBargraphPin = BARGRAPH_FIRST_PIN;
  secondaryBargraphPin = BARGRAPH_LAST_PIN;

  for (int i = BARGRAPH_FIRST_PIN; i <= BARGRAPH_LAST_PIN; i++) {
     shifter.setPin(i, LOW);
  }
}

/****************************************************
 * Remaining gun functions, including laser control *
 ****************************************************/
void slobloNormal()
{
  if (isFiring) {
    shifter.setPin(SLO_BLO_PIN, curLevel >  6 ? HIGH : LOW);
  }
  else {
    slobloState = !slobloState;
    shifter.setPin(SLO_BLO_PIN, slobloState ? HIGH : LOW);
  }
}

void guntopNormal()
{
   if (isFiring) {
    shifter.setPin(GUNTOP_PIN, HIGH);
   }
  else {
   shifter.setPin(GUNTOP_PIN, guntopState > 2 ? HIGH : LOW);
   if (++guntopState > 8) {
     guntopState = 0;
   }
  } 
}

void laserFiring()
{
  int rand = random(LASER_OPTIONS);
  digitalWrite(LASER_PIN, laserState ? HIGH : LOW);

  firing_laser.setInterval(laserState ? LASER_DURATIONS[rand] : LASER_DELAYS[rand]);
  laserState = !laserState;
}
