#ifndef Shifter_h
#define Shifter_h

// Include the standard types
#include <Arduino.h>

// Define the Shifter class
class Shifter
{
  public:
    // Constructor
    Shifter(int SER_Pin, int RCLK_Pin, int SRCLK_Pin, int Number_of_Registers);
   	void write();
   	void setPin(int index, boolean val);
   	void setAll(boolean val);
   	void clear();
    boolean isUpdateNeeded();



  private:
	int _SER_Pin;
  	int _RCLK_Pin;
  	int _SRCLK_Pin;
  	int _Number_of_Registers;
  	byte _shiftRegisters[25];
    boolean _updateNeeded;
};

#endif //Shifter_h