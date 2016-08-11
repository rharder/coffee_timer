// Title: Coffee Timer
// Author: Robert Harder
// Purpose: Attach to office coffee pot to report on age of the coffee

// https://github.com/openenergymonitor/EmonLib
#include "EmonLib.h"                   // Include Emon Library
EnergyMonitor emon1;                   // Create an instance

// By experiment figure out what an appropriate threshold would be
#define IRMS_THRESHOLD 0.20

// State machine
unsigned char state = 0;
#define STATE_UNKNOWN 0
#define STATE_BREWING 1
#define STATE_BREWED 2

// Where is the current sensor plugged in to Arduino
#define CURRENT_SENSOR_PIN A0

// Based on millis() once brewing ends
unsigned long coffee_birth = 0;

void setup()
{  
  Serial.begin(9600);
  
  emon1.current(CURRENT_SENSOR_PIN, 111.1); // Current: input pin, calibration.
  for( int i = 0; i < 10; i++ ){
    emon1.calcIrms(1480); // Flush initial bad reads
  }

  // do_threshold_experiment();
}


void loop()
{
  // Read the current sensor
  double Irms = emon1.calcIrms(1480);  // Calculate Irms only

  // Under threshold means coffee machine is off
  if( Irms < IRMS_THRESHOLD ){

    // It's off -- what was it's previous state?
    switch( state ){

      // Was brewing -- apparently just turned off.  Start clock.
      case STATE_BREWING:
        state = STATE_BREWED;
        coffee_birth = millis();
        break;

      // Was alread in Brewed state -- no change
      case STATE_BREWED: // No change
        break;

      // Not sure - call it Unknown
      default:
        state = STATE_UNKNOWN;
        coffee_birth = 0;
        break;
    } // end switch: state
    
  } else {  // Coffee pot is turned on

    // If we were in a state other than brewing, then this is new
    if( state != STATE_BREWING ){
      state = STATE_BREWING;
      coffee_birth = 0;
    }
  }

  // If there has been a rollover in millis() just set state to Unknown
  if( state != STATE_UNKNOWN && millis() < coffee_birth ){
    state = STATE_UNKNOWN;
  }

  update_display();  // Update visual display

  delay(1000);  // Don't really need this loop spinning any faster than this
} // end loop

void update_display(){
  
  switch( state ){
    case STATE_UNKNOWN:
      Serial.println("Coffee age: Unknown");
      break;
    case STATE_BREWING:
      Serial.println("Coffee brewing...");
      break;
    case STATE_BREWED:
      Serial.print("Coffee age: ");
      unsigned long seconds = coffee_age_seconds();
      unsigned long minutes = (seconds % 3600) / 60;
      unsigned long hours = seconds / 3600;
      unsigned long seconds_only = seconds % 60;

      // Find largest non-zero measurement to make human readable time
      if( hours > 0 ){ // 3 hr 27 min
        Serial.print(hours); Serial.print(" hr ");
        Serial.print(minutes); Serial.print(" min");
      } else if( minutes > 0 ){ // 2 min 45 sec
        Serial.print(minutes); Serial.print(" min ");
        Serial.print(seconds_only); Serial.print(" sec");
      } else { // 12 sec
        Serial.print(seconds_only); Serial.print(" sec");
      }
      Serial.println();
      break;
  } // end switch: state
} // end update_display


// Return the number of seconds since coffee was created
unsigned long coffee_age_seconds(){
  unsigned long mill = millis();

  // Confirm magnitudes before doing unsigned integer math
  if( mill > coffee_birth ){
    unsigned age_millis = millis() - coffee_birth;
    return age_millis / 1000;
    
  } else { // Something wrong with overflow or something else
    return 0;
  }
  
} // end coffee_age_seconds

// Only meant to be run during setup.
void do_threshold_experiment(){
  double Irms = 0;
  Serial.begin(9600);
  Serial.println("Turn machine on and off to see what a reasonable threshold would be.");
  while(true){
    Irms = emon1.calcIrms(1480);  // Calculate Irms only
    Serial.println(Irms);
  }
}

