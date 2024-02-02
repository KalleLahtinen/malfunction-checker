/**
 * Malfunction checking for an Arduino system
 */

#include <TM1638plus.h> //include the screen library
#include <EEPROM.h> // include the EEPROM library

// GPIO I/O pins on the Arduino connected to strobe, clock, data,
// pick on any I/O you want.
#define  STROBE_TM 4 // strobe = GPIO connected to strobe line of module
#define  CLOCK_TM 6  // clock = GPIO connected to clock line of module
#define  DIO_TM 7 // data = GPIO connected to data line of module
bool high_freq = false; //default false, If using a high freq CPU > ~100 MHZ set to true. 
 
// Constructor object (GPIO STB , GPIO CLOCK , GPIO DIO, use high freq MCU)
TM1638plus tm(STROBE_TM, CLOCK_TM , DIO_TM, high_freq);

// Define a map function for use with float values
#define fmap(value, oldmin, oldmax, newmin, newmax) (((value) - (oldmin)) * ((newmax) - (newmin)) / ((oldmax) - (oldmin)) + (newmin));

// Create constant valuables for pins
const int HYDRAULIC_PIN = 8;
const int POTENTIOMETER_PIN = A0;
const int RELAY_ON_PIN = 5;
const int DATA_LOGGER_PIN = 9;

// Declare global variables
int pot_value_old;
unsigned long last_read_pot = 0;
const unsigned long pot_read_period = 800;
float potentiometer_value;
long int milliseconds;
char screen_state[12];

// Amount of time relay is activated for
const int MANUAL_RELAY_TIME = 5000;
const int TIMER_RELAY_TIME = 12000;

// Max timer value
const long int TIMER_MAX = 10800000;

// Min timer value
const long int TIMER_MIN = 3600000;

float slope;
const int slope_memory_addr = 1;
float mustajarvi_variable;
const int mustajarvi_memory_addr = 6;

bool button_8_was_pressed;
bool button_7_was_pressed;
bool button_6_was_pressed;
bool button_5_was_pressed;
bool button_4_was_pressed;

 
void setup() {
  tm.displayBegin(); // Initialise display module
  Serial.begin(9600); // Initialize the potentiometer data rate

  pinMode(HYDRAULIC_PIN, INPUT);
  pinMode(POTENTIOMETER_PIN, INPUT);
  pinMode(RELAY_ON_PIN, OUTPUT);
  
  EEPROM_Read(&slope, slope_memory_addr);
  if(isnan(slope) || slope == 0) {
    slope = 0.6;
  }

  EEPROM_Read(&mustajarvi_variable, mustajarvi_memory_addr);
  if(isnan(mustajarvi_variable) || mustajarvi_variable == 0) {
    mustajarvi_variable = 3;
  }

  strcpy(screen_state, "time");
  bool button_was_pressed = false;
  read_potentiometer();
  calculate_new_time();

  tm.displayText("starting"); // Display "starting" on 8-segment displays

    // For i: 0 -> 7
  for(int i=0; i<8; i++) {
    tm.setLED(i, 1); // Turn LED at position i ON
    delay(100); // Wait 100ms
    tm.setLED(i, 0); // Turn LED at position i OFF
  }
  delay(500);
  tm.reset(); // Clear the display
}

// Send a signal to RELAY_ON_PIN
void activate_relay(int milliseconds)  {
  tm.reset(); // Clear the display and leds
  tm.displayText("RELE ON"); // Display "RELE ON" on 8-segment displays
  
  for(int i=0; i<8; i++) {
    tm.setLED(i, 1); // Turn LED at position i ON
    delay(50); // Wait 50ms
    tm.setLED(i, 0); // Turn LED at position i OFF
  }
  digitalWrite(RELAY_ON_PIN, HIGH);
  digitalWrite(DATA_LOGGER_PIN, HIGH);
  delay(milliseconds);
  digitalWrite(RELAY_ON_PIN, LOW);
  digitalWrite(DATA_LOGGER_PIN, LOW);
  calculate_new_time();
}

// Display an affirmation screen for risky buttons
bool confirm_choice(char* str) {
  tm.reset(); // Clear screen and leds
  tm.displayText("Confirm"); // Display "Confirm" on 8-segment displays
  delay(1000);
  
  tm.reset(); // Clear the display
  tm.displayText(str); // Display the given str on 8-segment displays
  delay(2000);
  
  tm.reset(); // Clear the display
  tm.setLED(0, 1); // Turn LED at position 1 ON
  tm.setLED(7, 1); // Turn LED at position 8 ON
  tm.displayText("JOO   EI"); // Display "JOO EI" on 8-segment displays

  while (true) {
    if (isButtonBeingPressed(1)) {
      tm.setLED(0, 0); 
      tm.setLED(7, 0);
      tm.reset();
      return true;

    } else if (isButtonBeingPressed(8))  {
      tm.setLED(0, 0); 
      tm.setLED(7, 0);
      tm.reset();
      return false;
    }
  }
}

// Write to EEPROM memory
void EEPROM_Write(float *num, int MemPos)
{
  byte ByteArray[4];
  memcpy(ByteArray, num, 4);
  for(int x = 0; x < 4; x++)
  {
    EEPROM.write((MemPos * 4) + x, ByteArray[x]);
  }
}

// Read from EEPROM memory
void EEPROM_Read(float *num, int MemPos)
{
  byte ByteArray[4];
  for(int x = 0; x < 4; x++)
  {
    ByteArray[x] = EEPROM.read((MemPos * 4) + x);    
  }
  memcpy(num, ByteArray, 4);
}

// This function will return true if a particular button n is currently being pressed.
boolean isButtonBeingPressed(int n){
 // Button 1 status shown by bit0 of the byte buttons returned by module.getButtons()
 // Button 2 status shown by bit1 or the byte buttons ...
 // Button 3 status shown by bit2...etc

 // n - the number of the button to be tested) should be an integer from 1 to 8
 if(n < 1 or n > 8) return false;

 // Read in the value of getButtons from the TM1638 module.
 byte buttons = tm.readButtons();

 // Which bit must we test for this button?
 int bitToLookAt = n - 1;

 // Read the value of the bit - either a 1 for button pressed, or 0 for not pressed.
 byte theValueOfTheBit = bitRead(buttons, bitToLookAt);

 // If the button is pressed, return true, otherwise return false.
 if(theValueOfTheBit == 1) 
   return true;
 else 
   return false;
}

// Calculates a new start value for timer where "time as hours = slope * potentiometer_value + mustajarvi_variable"
void calculate_new_time() {
  milliseconds = (-slope * potentiometer_value + mustajarvi_variable) * 3600 * 1000;

  if (milliseconds < TIMER_MIN) {
    milliseconds = TIMER_MIN;
  } else if (milliseconds > TIMER_MAX) {
    milliseconds = TIMER_MAX;
  }
}

// Displays current time left in timer
void display_time() {  
  int seconds = milliseconds / 1000;
  
  int minutes = seconds / 60;
  seconds %= 60;
  
  int hours = minutes / 60;
  minutes %= 60;

  tm.reset(); // Clear the display
  char buffer[40];
  sprintf(buffer, "%02d.%02d.%02d", hours, minutes, seconds);
  tm.displayText(buffer); // Display compiled screen info
}

// Displays current mustajarvi_variable and changes it in 0.01 increments with buttons 1 and 2
void display_mustajarvi() {
  if (isButtonBeingPressed(1)) {
    if (mustajarvi_variable > 2.01) {
      mustajarvi_variable = mustajarvi_variable - 0.01;
      calculate_new_time();
    }
  }
  if (isButtonBeingPressed(2)) {
    if (mustajarvi_variable < 4) {
      mustajarvi_variable = mustajarvi_variable + 0.01;
      calculate_new_time();
    }
  }
  int i = 100 * mustajarvi_variable;
  char s[20];

  tm.reset(); // Clear the display
  sprintf(s, "%02d.%02d", i / 100, i % 100);
  tm.displayText(s);
}

// Displays current slope and changes it in 0.01 increments with buttons 1 and 2
void display_slope() { 
  if (isButtonBeingPressed(1)) {
    if (slope > 0.51) {
      slope = slope - 0.01;
      calculate_new_time();
    }
  }
  if (isButtonBeingPressed(2)) {
    if (slope < 0.7) {
      slope = slope + 0.01;
      calculate_new_time();
    }
  }
  // Remove decimals and convert to integer for compatibility with sprintf function
  int i = 100 * slope;
  char s[20];

  tm.reset(); // Clear the display
  sprintf(s, "%02d.%02d", i / 100, i % 100);
  tm.displayText(s);
}

// Switches to the next screen state
void change_screen() {
  if (strcmp(screen_state, "time") == 0) {
    strcpy(screen_state, "mustajarvi");
    
  } else if (strcmp(screen_state, "mustajarvi") == 0) {
    strcpy(screen_state, "slope");
    EEPROM_Write(&mustajarvi_variable, mustajarvi_memory_addr);
    
  } else if (strcmp(screen_state, "slope") == 0) {
    strcpy(screen_state, "time");
    EEPROM_Write(&slope, slope_memory_addr);

  // Return for potentiometer value display to timer screen
  } else if (strcmp(screen_state, "potentiometer") == 0) {
    strcpy(screen_state, "time");
  }
}


// Change screen on button release if button 8 was pressed
void screen_change_btn() {
  if (isButtonBeingPressed(8)) {
        button_8_was_pressed = true;
    }
    // This if statement will only fire on the rising edge of the button input
    if (!isButtonBeingPressed(8) && button_8_was_pressed)  {
        // Reset the button press flag
        button_8_was_pressed = false;

        // Do the button action
        change_screen();
    }
}

// Recalculate time on button release if button 7 was pressed
void check_time_btn() {
  if (isButtonBeingPressed(7)) {
        button_7_was_pressed = true;
    }
    // This if statement will only fire on the rising edge of the button input
    if (!isButtonBeingPressed(7) && button_7_was_pressed)  {
        // Reset the button press flag
        button_7_was_pressed = false;

        // Do the button action
        calculate_new_time();
    }
}

void display_potentiometer_btn() {
  if (isButtonBeingPressed(6)) {
        button_6_was_pressed = true;
    }
    // This if statement will only fire on the rising edge of the button input
    if (!isButtonBeingPressed(6) && button_6_was_pressed)  {
        strcpy(screen_state, "potentiometer");

        // Reset the button press flag
        button_6_was_pressed = false;

        int data = analogRead(POTENTIOMETER_PIN);

        // Display the potentiometer value
        tm.reset(); // Clear the display
        tm.displayIntNum(data); // Display potentiometer value on 8-segment displays
    }
}

void check_relay_btn() {
  if (isButtonBeingPressed(5)) {
    button_5_was_pressed = true;
  }
  // This if statement will only fire on the rising edge of the button input
    if (!isButtonBeingPressed(5) && button_5_was_pressed)  {
      // Ask for confirmation with buttons 1 and 8 in risky functionalities
        if (confirm_choice("RELE?")) {
          activate_relay(MANUAL_RELAY_TIME);

        }
        // Reset the button press flag
        button_5_was_pressed = false; 
    }
}

void clear_memory_btn() {
  if (isButtonBeingPressed(4)) {
        button_4_was_pressed = true;
    }
    // This if statement will only fire on the rising edge of the button input
    if (!isButtonBeingPressed(4) && button_4_was_pressed)  {
      // Ask for confirmation with buttons 1 and 8 in risky functionalities
      if (confirm_choice("clr mEm?")) {
        for (int i = 0 ; i < EEPROM.length() ; i++) {
          EEPROM.write(i, 0);
        }
        tm.reset(); // Clear the display and leds
        tm.displayText("MEM CLRD"); // Display "MEM CLRD" on 8-segment displays
        
        // For i: 0 -> 7
        for(int i=0; i<8; i++) {
          tm.setLED(i, 1); // Turn LED at position i ON
          delay(50); // Wait 50ms
          tm.setLED(i, 0); // Turn LED at position i OFF
        }
      }
      // reset the button press flag
      button_4_was_pressed = false;
    }
}

// Reads potentiometer value and saves it as a value in range 0.00-5.00
void read_potentiometer() {
  // Read potentiometer once every pot_read_period
  if (abs(last_read_pot - millis()) >= pot_read_period) {
    int pot_value = analogRead(POTENTIOMETER_PIN);

    if (pot_value < 200) {
      pot_value = 200;
    } else if (pot_value > 900) {
        pot_value = 900;
    }

    if ((abs(pot_value - pot_value_old)) >= 80) {
      last_read_pot = millis();
      pot_value_old = pot_value;
      potentiometer_value = fmap(pot_value, 200, 900, 0.00f, 5.00f);
      calculate_new_time();
    }
  }
}

void loop() {
  read_potentiometer();

  // Display the current screen state
  if (strcmp(screen_state, "time") == 0 ) {
    display_time();

  } else if (strcmp(screen_state, "mustajarvi") == 0) {
    display_mustajarvi();
    
  } else if (strcmp(screen_state, "slope") == 0) {
    display_slope();

  } else if (strcmp(screen_state, "potentiometer") == 0) {
    display_potentiometer_btn();
  }
  
  // Change screen on button release if button 8 was pressed
  screen_change_btn();

  // Recalculate time if button 7 was pressed
  check_time_btn();

  // Display potentiometer value if button 6 was pressed
  display_potentiometer_btn();

  // Manually send a signal to RELAY_ON_PIN if button 5 was pressed and confirmed
  check_relay_btn();

  // Clear EEPROM memory if button 4 was pressed and confirmed
  clear_memory_btn();

  // Reset time if hydraulic was activated
  if (digitalRead(HYDRAULIC_PIN) == LOW) {
    tm.reset(); // Clear the display
    tm.setLED(0, 1); // Turn LED at position i ON
    tm.displayText("NOLLAUS"); // Display "NOLLAUS" on 8-segment displays
    calculate_new_time();
    delay(5000);
  } else {
    tm.setLED(0, 0); // Turn LED at position i OFF
  }

  // Run loop 10 times per second
  delay(100);
  milliseconds = milliseconds - 112;

  // Send a signal to RELAY_ON_PIN if timer runs out
  if(milliseconds <= 0) {
    activate_relay(TIMER_RELAY_TIME);
  }
}
