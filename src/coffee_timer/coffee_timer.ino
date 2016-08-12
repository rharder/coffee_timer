// Title: Coffee Timer
// Author: Robert Harder
// Purpose: Attach to office coffee pot to report on age of the coffee
// Repository: https://github.com/rharder/coffee_timer

// Configurable pins, etc for this implementation:
/**
 * Configurable pins, values, etc for your implementation.
 * 
 * Note: Arduino Nano I2C pins are A4-SDA, A5-SCL
 */
#define PIEZZO_SPEAKER_PIN 8
#define CURRENT_SENSOR_PIN A0
#define LCD_I2C_ADDR 0x27
#define MINUTES_AFTER_WHICH_DROP_SECONDS 5
#define DAYS_AFTER_WHICH_DROP_HOURS 2
#define SAMPLES_NUM 10
#define IRMS_THRESHOLD 1.4  

// 
/**
 * By experiment figure out what an appropriate threshold would be,
 * and update the IRMS_THRESHOLD for whatever works for you.
 * 
 * Nice todo item:  Have a reactive system that detects radical
 * changes in sensor values.  Pull out my statistics book.
 */
double samples_val[SAMPLES_NUM];
unsigned int sample_pos = 0;


/**
 * Use the Energy Monitor library to read the analog sensor.
 * https://github.com/openenergymonitor/EmonLib
 */
#include "EmonLib.h"                   // Include Emon Library
EnergyMonitor emon1;                   // Create an instance


/**
 * Using 16x2 LCD screen to display data.
 */
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(LCD_I2C_ADDR,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display
const String BLANK_LINE = String("                ");


/**
 * Play music when coffee is done.
 */
#include "pitches.h"
const int CHARGE_FANFARE[] = { NOTE_G4, NOTE_C4, NOTE_E5, NOTE_G5, NOTE_E5, NOTE_G5 };
const int CHARGE_FANFARE_DURATIONS[] = { 8,8,8,4,8,2 };


/**
 * States for our program, only three.
 */
#define STATE_UNKNOWN 0
#define STATE_BREWING 1
#define STATE_BREWED 2
unsigned char state = STATE_UNKNOWN;



/**
 * Keep track of when the coffee was finished
 * being brewed (born), and account for rollovers
 * of the millis() timer.
 */
unsigned long coffee_birth_rollovers = 0;
unsigned long coffee_birth_millis = 0;
unsigned long rollover_count = 0;
unsigned long prev_millis = 0;


/**
 * Initial setup.
 */
void setup()
{  
  //Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  lcd_set_line(0, "Getting baseline");
  lcd_set_line(1, "reading...");

  
  emon1.current(CURRENT_SENSOR_PIN, 111.1); // Current: input pin, calibration.
  for( int i = 0; i < 5; i++ ){
    emon1.calcIrms(1480); // Flush a few initial bad reads
    delay(2);
  }
  // Fill our running average sample list with real values
  for( unsigned int i = 0; i < SAMPLES_NUM; i++ ){
    sample_make_observation();
    delay(2);
  }

  // If you want to see what values you're getting when your coffee
  // pot is on or off, uncomment this line, and the program will display
  // the running average of the raw readings.  Turn your pot on and off,
  // and see what looks like a good threshold to use.
  // When done, re-comment the line below.
  //do_threshold_experiment();  // Find out what threshold should be
}

/**
 * Main loop.
 */
void loop()
{
  unsigned long loop_start = millis();

  // Has there been a rollover?
  if( loop_start < prev_millis ){
    rollover_count++;
  }
  prev_millis = loop_start;

  // Read the current sensor
  sample_make_observation();
  double Irms = sample_get_average();

  // Under threshold means coffee machine is off
  if( Irms < IRMS_THRESHOLD ){

    // It's off -- what was it's previous state?
    switch( state ){

      case STATE_BREWING: // Was "brewing"...
        // Coffee is ready!
        // 
        //    ( (
        //     ) )
        //   ........
        //   |      |]
        //   \      /    Jen Carlson
        //    `----'
        // http://www.ascii-art.de/ascii/c/coffee.txt
        //
        // Was brewing -- apparently just turned off.  Start clock.
        state = STATE_BREWED;
        coffee_birth_millis = millis();
        coffee_birth_rollovers = rollover_count;
        play_charge_fanfare(PIEZZO_SPEAKER_PIN);
        break;

      
      case STATE_BREWED: // Was "brewed"...
        // Was alread in Brewed state -- no change
        break;

      case STATE_UNKNOWN: // Was "unknown"
        break;

      default:
        state = STATE_UNKNOWN;
        break;
    } // end switch: state
    
  } else {  // Coffee pot is turned on

    // If we were in a state other than brewing, then this is new
    if( state != STATE_BREWING ){
      state = STATE_BREWING;
    }
  }

  // Update visual display
  update_display();  

  // Minor delay on principle, but we do want to get
  // back to reading the sensor quickly.
  delay(2);
  
} // end loop




/**
 * Make a single observation of the sensor and
 * add it to the running list of observations.
 */
void sample_make_observation(){
  samples_val[sample_pos] = emon1.calcIrms(1480);
  sample_pos++;
  sample_pos = sample_pos % SAMPLES_NUM;
}

/**
 * Get the current running average of the sensor samples.
 */
double sample_get_average(){
  double sum = 0;
  for( unsigned int i = 0; i < SAMPLES_NUM; i++ ){
    sum += samples_val[i];
  }
  return sum / SAMPLES_NUM;
}

/**
 * Return the number of seconds since coffee was created.
 * Accounts for millis() rollovers.
 */
unsigned long coffee_age_seconds(){
  unsigned long mill = millis();  // Just read this once

  // Are we on the same rollover?
  // [....B====M.....] millis - Birth
  if( rollover_count == coffee_birth_rollovers ){
    return (mill - coffee_birth_millis) / 1000;
    
  } else { // Deal with rollovers
    unsigned long seconds = 0;
    unsigned long temp = 0;
    
    // Add seconds from initial coffee birth to its first rollover
    // [....B=========R] Rollover - Birth
    seconds += (0xFFFFFFFF - coffee_birth_millis) / 1000;
    Serial.print(String(seconds) + String(" "));

    // Add up any intervening rollovers in their entirety
    // [==============R] Full span of 32-bit unsigned int
    for( unsigned long i = 1; i < (rollover_count - coffee_birth_rollovers); i++ ){
      seconds += 0xFFFFFFFF / 1000;
    } // end for: in-between rollovers
    Serial.print(String(seconds) + String(" "));

    // Add up seconds from current rollover zero to millis()
    // [====M..........] millis() - zero
    seconds += mill / 1000;
    Serial.println(String(seconds) + String(" "));

    return seconds;
  }

} // end coffee_age_seconds



/**
 * Determine what the display should be saying.
 */
void update_display(){
  
  switch( state ){
    
    case STATE_UNKNOWN:
      lcd_set_line(0, "Age of coffee:");
      lcd_set_line(1, "Unknown");
      break;
      
    case STATE_BREWING:
      lcd_set_line(0, "Brewing coffee...");
      lcd_set_line(1, BLANK_LINE);
      break;
      
    case STATE_BREWED:
      unsigned long seconds_total = coffee_age_seconds();
      unsigned long seconds = seconds_total % 60;
      unsigned long minutes = (seconds_total / 60) % 60;
      unsigned long hours = (seconds_total / 3600) % 24;
      unsigned long days = seconds_total / (3600*24);


      // Make human readable time
      String age;
      if( days >= DAYS_AFTER_WHICH_DROP_HOURS ){
        age = String(days) + String(" days");
        
      } else if( days > 0 ){
        age = String(days) + String(" days") + String(hours) + String(" hr");
        
      } else if( hours > 0 ){ // 3 hr 27 min
        age = String(hours) + String(" hr ") + String(minutes) + String(" min" );
        
      } else if( minutes >= MINUTES_AFTER_WHICH_DROP_SECONDS ){ // 22 min
        age = String(minutes) + String(" min ");
        
      } else if( minutes > 0 ){ // 2 min 45 sec
        age = String(minutes) + String(" min ") + String(seconds) + String(" sec" );
        
      } else { // 12 sec
        age = String(seconds) + String(" sec" );
        
      }
      lcd_set_line(0, "Age of coffee:");
      lcd_set_line(1, age);
      break;
  } // end switch: state
} // end update_display


/**
 * Sets a line of text on the LCD.
 * Zero indexed.
 */
void lcd_set_line(unsigned int lineNum, String newLine){
  static String prev[2];
  if( prev[lineNum] == NULL || !prev[lineNum].equals(newLine)){
    lcd.setCursor(0,lineNum);
    lcd.print(newLine);
    lcd.print(BLANK_LINE); // Fill up the rest with spaces.
    prev[lineNum] = String(newLine);
  }
}


/**
 * To use for finding threshold of coffee pot on/off.
 * Instructions in setup() notes.
 */
void do_threshold_experiment(){
  double Irms = 0;
  Serial.begin(9600);
  Serial.println("Turn machine on and off to see what a reasonable threshold would be.");
  while(true){
    sample_make_observation();
    Irms = sample_get_average();
    Serial.println(Irms);
    lcd_set_line(0, String(Irms));
    lcd_set_line(1, String(Irms));
    delay(10);
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



