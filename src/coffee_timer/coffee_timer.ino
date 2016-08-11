// Title: Coffee Timer
// Author: Robert Harder
// Purpose: Attach to office coffee pot to report on age of the coffee
// Repository: https://github.com/rharder/coffee_timer

// Configurable pins, etc for this physical implementation:
#define PIEZZO_SPEAKER_PIN 8
#define CURRENT_SENSOR_PIN A0
#define LCD_I2C_ADDR 0x27

// Arduino Nano I2C pins are A4-SDA, A5-SCL

// https://github.com/openenergymonitor/EmonLib
#include "EmonLib.h"                   // Include Emon Library
EnergyMonitor emon1;                   // Create an instance


// May switch first screen over at another time. Ought to.
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(LCD_I2C_ADDR,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display
const String BLANK_LINE = String("                ");


// Play music when coffee is done
#include "pitches.h"
const int CHARGE_FANFARE[] = { NOTE_G4, NOTE_C4, NOTE_E5, NOTE_G5, NOTE_E5, NOTE_G5 };
const int CHARGE_FANFARE_DURATIONS[] = { 8,8,8,4,8,2 };


// By experiment figure out what an appropriate threshold would be
#define IRMS_THRESHOLD 1.5  // when plugged into usb, 0.2

// State machine
unsigned char state = 0;
#define STATE_UNKNOWN 0
#define STATE_BREWING 1
#define STATE_BREWED 2


// Based on millis() once brewing ends
unsigned long coffee_birth = 0;



void setup()
{  
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  lcd.print("Startup...");
  
  emon1.current(CURRENT_SENSOR_PIN, 111.1); // Current: input pin, calibration.
  for( int i = 0; i < 10; i++ ){
    emon1.calcIrms(1480); // Flush initial bad reads
  }

  //do_threshold_experiment();  // Find out what threshold should be
}

/**
 * Main loop.
 */
void loop()
{
  unsigned long loop_start = millis();
  //Serial.print("loop start state ");
  //Serial.print(state);
  
  // Read the current sensor
  double Irms = emon1.calcIrms(1480);  // Calculate Irms only

  // Under threshold means coffee machine is off
  if( Irms < IRMS_THRESHOLD ){

    // It's off -- what was it's previous state?
    switch( state ){

      // Coffee is ready!
      // Was brewing -- apparently just turned off.  Start clock.
      case STATE_BREWING:
        state = STATE_BREWED;
        coffee_birth = millis();
        play_charge_fanfare(PIEZZO_SPEAKER_PIN);
        break;

      // Was alread in Brewed state -- no change
      case STATE_BREWED: // No change
        break;

      case STATE_UNKNOWN:
        break;

      // Not sure - call it Unknown
      default:
        state = STATE_UNKNOWN;
        coffee_birth = 0;
        //Serial.println("A. RESET COFFEE TO 0");
        break;
    } // end switch: state
    
  } else {  // Coffee pot is turned on

    // If we were in a state other than brewing, then this is new
    if( state != STATE_BREWING ){
      state = STATE_BREWING;
      coffee_birth = 0;
      //Serial.println("B. RESET COFFEE TO 0");
    }
  }

  // If there has been a rollover in millis() just set state to Unknown
  if( state != STATE_UNKNOWN && millis() < coffee_birth ){
    state = STATE_UNKNOWN;
  }

  update_display();  // Update visual display

  // Don't really need this loop spinning any faster than this
  if( millis() - loop_start < 1000 ){
    delay( 1000 - (millis() - loop_start) );
  }
  
} // end loop


/**
 * Determine what the display should be saying.
 */
void update_display(){
  
  switch( state ){
    
    case STATE_UNKNOWN:
      Serial.println("Coffee age: Unknown");
      lcd_set_line(0, "Age of coffee:");
      lcd_set_line(1, "Unknown");
      break;
      
    case STATE_BREWING:
      Serial.println("Coffee brewing...");
      lcd_set_line(0, "Brewing coffee...");
      lcd_set_line(1, BLANK_LINE);
      break;
      
    case STATE_BREWED:
      Serial.print("Age of coffee: ");
      unsigned long seconds = coffee_age_seconds();
      unsigned long minutes = (seconds % 3600) / 60;
      unsigned long hours = seconds / 3600;
      unsigned long seconds_only = seconds % 60;

      //Serial.print("(");Serial.print(seconds);Serial.print(") ");

      // Find largest non-zero measurement to make human readable time
      String age;
      if( hours > 0 ){ // 3 hr 27 min
        age = String(hours) + String(" hr ") + String(minutes) + String(" min" );
      } else if( minutes >= 10 ){ // 22 min
        age = String(minutes) + String(" min ");
      } else if( minutes > 0 ){ // 2 min 45 sec
        age = String(minutes) + String(" min ") + String(seconds_only) + String(" sec" );
      } else { // 12 sec
        age = String(seconds_only) + String(" sec" );
      }
      lcd_set_line(0, "Age of coffee:");
      lcd_set_line(1, age);
      Serial.println(age);
      break;
  } // end switch: state
} // end update_display


/**
 * Sets a line of text on the LCD.
 */
void lcd_set_line(unsigned int lineNum, String newLine){
  static String prev[2];
  if( prev[lineNum] == NULL || !prev[lineNum].equals(newLine)){
    lcd.setCursor(0,lineNum);
    lcd.print(newLine);
    prev[lineNum] = String(newLine);
  }
}


/**
 * Return the number of seconds since coffee was created.
 */
unsigned long coffee_age_seconds(){
  unsigned long mill = millis();

  // Confirm magnitudes before doing unsigned integer math
  if( mill > coffee_birth ){
    unsigned long age_millis = mill - coffee_birth;
    Serial.print("[");Serial.print(age_millis);Serial.print("]");
    return age_millis / 1000;
    
  } else { // Something wrong with overflow or something else
    return 999;
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
    lcd_set_line(1, String(Irms));
  }
}

/**
 * Musical announcement when coffee is done brewing.
 * 
 * @param pin The Arduino output pin on which to play the tone.
 */
void play_charge_fanfare(int pin){
    // iterate over the notes of the melody:
  for (int thisNote = 0; thisNote < 6; thisNote++) {

    // to calculate the note duration, take one second
    // divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / CHARGE_FANFARE_DURATIONS[thisNote];
    tone(pin, CHARGE_FANFARE[thisNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    // stop the tone playing:
    noTone(pin);
  }
}



