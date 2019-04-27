// dczia 2018 proto dos test firmware
// combines all major hardware aspects (led, oled, keypad, ble)

// split into functional regions
//#include "dczia26.h"
#include "dczia26_keypad.h"
#include "dczia26_led.h"
#include "dczia26_oled.h"
#include "dczia26_ble.h"
#include "dczia26_sd.h"

#define COLOR_GREEN   HslColor(120.0 / 360.0, 1.0, 0.5)
#define COLOR_BLUE    HslColor(240.0 / 360.0, 1.0, 0.05)
#define COLOR_RED     HslColor(10.0 / 360.0, 1.0, 0.5)
#define COLOR_PURPLE  HslColor(270.0 / 360.0, 1.0, 0.5)

#define ZINTH_ADD_ON_ADDR 0x021A // zinthesizer i2c address

// Global variables
Adafruit_SSD1306   *oled = NULL; // uses v3.xx from "esp8266 and esp32 oled driver for ssd1306 display" (https://github.com/ThingPulse/esp8266-oled-ssd1306)
Keypad             *keys = NULL; // currently customized and included within project (will update to forked lib later)
hw_timer_t         *timer = NULL;

volatile char lastKey = NO_KEY;

char getLastKey(void);
void displayUpdate(std::string message);
void runDefaultAnimations(void);

void IRAM_ATTR onTimer() {
  char tempKey = keys->getKey();
  if(tempKey != NO_KEY){
    lastKey = tempKey;
  }
}

// Setup the board
void setup(void) {
  // init system debug output
  Serial.begin(115200);

  // Call class constructors
  Serial.println("Constructing...");
  keys = keypad_setup();
  delay(1);

  //Setup LEDS
  strip.Begin();
  strip.Show();
  startupAnimation();
  strip.Show();

  // Setup the key scan interrupt
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000, true);
  timerAlarmEnable(timer);

  // Setup the OLED
  oled = oled_setup();

  // Setup the bluetooth
  // COMMENT THIS OUT if your board gets stuck in a bootloop, or add a 100uF capacitor to C21 (boards sold on Tindie alreay have this fix)
  ble_setup();

  // call welcome screen (once)
  oled_welcome(oled);

  // Set a random seed
  SetRandomSeed();

  // Detect and setup a SD card
  SDSetup(oled);

  // done with init fuction
  Serial.println("Done With Setup!");
}

// in arduino world, "loop()" is called over and over and over and ...
// you get the idea... we don't need to "while(1)" ourselves...
void loop(void) {

  // Store what mode we're currently in
  static char mode = '1';
  static char oldMode = '1';
  static uint8_t currPressed = 16;

  // Determine if we've changed the mode.  Use this to initialize the animations
  static boolean newmode = true;

  // Is menu mode selected?
  if (keys->getState('D') == HOLD) {
      // User wants to select a new mode, jump to menu mode
      if (mode == '7') {
        // turn off APU if previously in zinth mode
        Wire.beginTransmission(ZINTH_ADD_ON_ADDR);
        Wire.write(129); // 129: note off
        Wire.write(100); // volume doesn't really matter
        Wire.endTransmission();
      }
      mode = 'D';
  }

  if(oldMode != mode){
    Serial.printf("Mode is %c (%d)\r\n", mode, mode);
    oldMode = mode;
  }

  // Run the code associated with the particular command
  switch (mode) {
    case '1': // Reserved for Light Mode
    case '8': // Reserved for Function Mode
    case '9': // Reserved for Function Mode
    case '*': // Reserved for Funciton Mode
    default:
      // Set the mode message
      if(newmode){
        displayUpdate("Hold Bottom Right\nfor Main Menu");
        newmode = false;
      }
      runDefaultAnimations();
      break;

    case 'D':
      {
        // Set the mode message
        if(newmode){
          displayUpdate("Menu");
          newmode = false;
        }
  
        // Swap modes to whatever the last key pressed was
        char key = getLastKey();
        if(key != NO_KEY){
          mode = key;
          newmode = true;
        }
  
        //Default Animation Loop
        if (animations.IsAnimating()) {
          // The normal loop just needs these two to run the active animations
          animations.UpdateAnimations();
          strip.Show();
        } else {
          // No animation runnning, start some
          FadeInFadeOutRinseRepeat(0.05f); // 0.0 = black, 0.25 is normal, 0.5 is bright
        }
      }
      break;

    //Light modes:
    //1, 2, 3, A
    //4, 5, 6, B
    //7
    case '2':
      // Set the mode message
      if(newmode){
        displayUpdate("Bright Rainbow");
        newmode = false;
      }

      //Default Animation Loop
      if (animations.IsAnimating()) {
        // The normal loop just needs these two to run the active animations
        animations.UpdateAnimations();
        strip.Show();
      } else {
        // No animation runnning, start some
        FadeInFadeOutRinseRepeat(.5f); // 0.0 = black, 0.25 is normal, 0.5 is bright
      }
      break;

    case 'B': {
      // Set the mode message
      if(newmode){
        displayUpdate("Pixel Picker");
        newmode = false;
      }

      if (keys->getState('D') != HOLD) {
        // Turn off any animations
        animations.StopAnimation(0);
        delay(1);

        // Get a copy of the keymap
        uint16_t map = 0;
        for(uint8_t i = 0; i < 4; i++){
          map |= ((keys->bitMap[i] & 0x0F) << (i * 4));
        }

        // Determine if a pixel needs to be on or off
        for(uint8_t i = 0; i < 16; i++){
          if((map >> i) & 0x01){
            // Key is down
            strip.SetPixelColor(i, COLOR_GREEN);
          }
          else{
            // Key is up
            strip.SetPixelColor(i, COLOR_BLUE);
          }
        }
        
        strip.Show();
      }
      break;
    }

    case '7': {
      // Set the mode message
      if(newmode){
        displayUpdate("Zinthesizer");
        for (uint16_t pixel = 0; pixel < 16; pixel++) { strip.SetPixelColor(pixel, COLOR_RED); }
        newmode = false;
      }

      if (keys->getState('D') != HOLD) {
        // Turn off any animations
        animations.StopAnimation(0);
        delay(1);

        // Get a copy of the keymap
        uint16_t map = 0;
        for(uint8_t i = 0; i < 4; i++){
          map |= ((keys->bitMap[i] & 0x0F) << (i * 4));
        }

        // Determine if a pixel needs to be on or off
        for(uint8_t i = 0; i < 16; i++){
          if((map >> i) & 0x01){
            // Key is down
            if (i != currPressed) {
              // set old key back to red
              strip.SetPixelColor(currPressed, COLOR_RED);
              
              // handle new key press
              currPressed = i;
              strip.SetPixelColor(i, COLOR_PURPLE);
              // trigger synth val by midi note
              Wire.beginTransmission(ZINTH_ADD_ON_ADDR);
              Wire.write(52+i); // midi note
              Wire.write(100); // volume
              Wire.endTransmission();
            } else {
              // don't change anything
              break;
            }
          } else if (i == currPressed) {
            // Key is up
            currPressed = 16;
            strip.SetPixelColor(i, COLOR_RED);
            Wire.beginTransmission(ZINTH_ADD_ON_ADDR);
            Wire.write(129); // 129: note off
            Wire.write(100); // volume doesn't really matter
            Wire.endTransmission();
          }
        }
        
        strip.Show();
      } else {
        // turn off APU before quitting
        Wire.beginTransmission(ZINTH_ADD_ON_ADDR);
        Wire.write(129); // 129: note off
        Wire.write(100); // volume doesn't really matter
        Wire.endTransmission();
      }
      
      break;
    }

    case 'A':
      // Set the mode message
      if (newmode){
        displayUpdate("Connection Machine");
        animations.StopAnimation(0);
        newmode = false;
      }
      //Connection Machine animation
      if (animations.IsAnimating()) {
        animations.UpdateAnimations();
        strip.Show();
      } else {
        PickRandom(.3);
      }
      break;

    case '4':
      // Set the mode message
      if (newmode){
        displayUpdate("Random Mode");
        animations.StopAnimation(0);
        newmode = false;
      }
   
      if (animations.IsAnimating()) {
        animations.UpdateAnimations();
        strip.Show();
      } else {
        Random(.2);
      }
      break;

    case '5':
      // Set the mode message
      if (newmode){
        displayUpdate("Light Walk Mode");
        animations.StopAnimation(0);
        newmode = false;
      }
        
      if (animations.IsAnimating()) {
        animations.UpdateAnimations();
        strip.Show();
      } else {
        LightIteration(.3);
      }
      break;

    case '3':
      // Set the mode message
      if (newmode){
        displayUpdate("Party Mode");
        animations.StopAnimation(0);
        newmode = false;
      }
        
      if (animations.IsAnimating()) {
        animations.UpdateAnimations();
        strip.Show();
      } else {
        PartyMode(.1);
      }
      break;

    case '6':
      // Set the mode message
      if (newmode) {
        displayUpdate("Color Waves");
        animations.StopAnimation(0);
        newmode = false;
      } else {
        ColorWaves(.1);
      }
      break;

    case '#':
      // BLE Scanning project

      if (newmode) {
        displayUpdate("BLE Scanning");
        newmode = false;
        
        int bleResults[3];
        
        oled->clearDisplay();
        oled->setCursor(0, 0);
        oled->setTextSize(2);
        oled->println("Scanning..");
        oled->display();
        
        ble_scan_dczia(bleResults);
        
        oled->clearDisplay();
        oled->setTextSize(1);
        oled->setCursor(0, 0);
        oled->println("- BLE Scan Results -");
        oled->print("BLE Devices: ");
        oled->print(bleResults[2]);
        oled->print("\n");
        oled->print("Defcon26 Badges: ");
        oled->print(bleResults[0]);
        oled->print("\n");
        oled->print("DCZia Badges: ");
        oled->print(bleResults[1]);
        oled->display();
      }
      runDefaultAnimations();
      break;

    case 'C':
      oled_displaytest(oled);
      // go back to menu
      mode = 'D';
      break;

    case '0':
      // Credits
      if (newmode) {
        oled_displayCredits(oled);
        //Put it back on the main menu...
        mode = 'D';
      }
      runDefaultAnimations();
      break;

  }

}

/**
 * Get the last key pressed
 */
char getLastKey(void){
  char retKey = lastKey;
  lastKey = NO_KEY;
  return retKey;
}

/**
 * Update the OLED screen
 */
void displayUpdate(std::string message){
    oled->clearDisplay();
    oled->setCursor(0, 0);
    oled->setTextSize(2);
    oled->println(" -DCZia26-");
    oled->setTextSize(1);
    oled->println(message.c_str());
    oled->display();
}

/**
 * Run the default animation - low intensity rainbow
 */
void runDefaultAnimations(void){
  //Default Animation Loop
  if (animations.IsAnimating()) {
    // The normal loop just needs these two to run the active animations
    animations.UpdateAnimations();
    strip.Show();
  } else {
    // No animation runnning, start some
    FadeInFadeOutRinseRepeat(.05f); // 0.0 = black, 0.25 is normal, 0.5 is bright
  }
}
