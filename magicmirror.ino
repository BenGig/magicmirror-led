
#include "FastLED.h"

#include <pins_arduino.h>

#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
// Connection and arrangement
// Arduino Nano
#define LEDOUT_L 2
#define LEDOUT_R 3
#define MOTIONIN A4
#define MOTIONOUT A3 // to Raspi
#define BRIGHTNESS_LOW A2  // Raspi
#define BRIGHTNESS_MED A1 // Raspi
#define BRIGHTNESS_HIGH A0 // Raspi
// also VIN and GND!

#define NUM_LEDS 14 // Half height of active LEDs! LEDs are symmetric top-bottom
#define LED_GAP 2 // Unused gap, to be skipped
// Brightness level range 0-3, is binary encoded, 2 pins for 2 bits
#define MAX_LEVEL 3 // logical brightness levels

#define LED_MAX_POWER 255 // max. LED brightness (per color)
const int STEPS_PER_LEVEL = 85; // change between brightness levels (255/levels)
const int STEP_INTERVAL = 1000;

const int all_leds = NUM_LEDS*2+LED_GAP;
CRGB leds_l[all_leds];
CRGB leds_r[all_leds];

int brightness = 0; // 0 - 3
int brightness_new = 0;

const int LED_FRAC_R = 255;
const int LED_FRAC_G = 208;
const int LED_FRAC_B = 200;

int incomingByte = 0;

// LED brigthness is not linear, scale from standard interval 85
int scale(int raw) {
  return 0.0035*raw*raw;
}

int shift_vals[NUM_LEDS];

bool motionDetected() {
  static unsigned long lastMotion = 0;
  if (digitalRead(MOTIONIN) == HIGH) {
    lastMotion = millis();
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(MOTIONIN, INPUT);
  pinMode(MOTIONOUT, OUTPUT);
  pinMode(LEDOUT_L, OUTPUT);
  pinMode(LEDOUT_R, OUTPUT);
  pinMode(BRIGHTNESS_LOW, INPUT_PULLUP);
  pinMode(BRIGHTNESS_MED, INPUT_PULLUP);
  pinMode(BRIGHTNESS_HIGH, INPUT_PULLUP);

  digitalWrite(MOTIONOUT, LOW);

  FastLED.addLeds<NEOPIXEL, LEDOUT_L>(leds_l, (NUM_LEDS+LED_GAP)*2);
  FastLED.addLeds<NEOPIXEL, LEDOUT_R>(leds_r, (NUM_LEDS+LED_GAP)*2);

  for (int dot = 0; dot < NUM_LEDS*2+LED_GAP; dot++) {
    leds_l[dot].setRGB(0,0,0);
    leds_r[dot].setRGB(0,0,0);
  }
  FastLED.show();
}

void loop() {
  // motion detected, signal Raspi
  digitalWrite(MOTIONOUT, digitalRead(MOTIONIN));
    
  // read current brightness level from Raspi; highest wins
  brightness_new = 0;
  if (digitalRead(BRIGHTNESS_LOW) == LOW) {
    brightness_new = 1;
  }
  if (digitalRead(BRIGHTNESS_MED) == LOW) {
    brightness_new = 2;
  }
  if (digitalRead(BRIGHTNESS_HIGH) == LOW) {
    brightness_new = 3;
  }

  if (brightness != brightness_new) {
    brightness = shift_led(brightness, brightness_new);
  }
}

void set_led(int led, int bri) {
  int led_r = LED_FRAC_R*scale(bri)/255;
  int led_g = LED_FRAC_G*scale(bri)/255;
  int led_b = LED_FRAC_B*scale(bri)/255;
  // Symmetry: 2 strips left/right, each mirror symmetric
  leds_l[led].setRGB(led_r, led_g, led_b);
  leds_r[led].setRGB(led_r, led_g, led_b);
  leds_l[all_leds - led -1].setRGB(led_r, led_g, led_b);
  leds_r[all_leds - led -1].setRGB(led_r, led_g, led_b);
}

// prepare a ramp which is -STEPS_PER_LEVEL for outer led and 0 for inner led
void init_shiftvalues() {
  for (int i=1; i<=NUM_LEDS; i++) {
    shift_vals[i-1] = STEPS_PER_LEVEL*(i-NUM_LEDS)/NUM_LEDS;
  }
}
void move_shiftvalues() {
  for (int i=0; i<NUM_LEDS; i++) {
    shift_vals[i] = shift_vals[i]+1;
  }
}

// led power [0-255] = level[0, 85, 170, 255] + (
int shift_led(int current, int target) {
  if (current != target) {
    // blending takes 1 second per brightness level and
    // goes from current to target level

    // set ramp for level shifting
    init_shiftvalues();
    // calculate level steps down, or up
    int shift_direction = (target - current) / abs(target - current); // should be +1 or -1
    int level_change = (target - current);


    int base_level = current*STEPS_PER_LEVEL;
    // process desired level changes
    for (int absolute_level = current; absolute_level != target+shift_direction; absolute_level = absolute_level+shift_direction) {
      int level = current - absolute_level;
      int minLevel; int maxLevel;
      if (shift_direction > 0) {
        minLevel = STEPS_PER_LEVEL*current;
        maxLevel = STEPS_PER_LEVEL*target;
      } else {
        minLevel = STEPS_PER_LEVEL*target;
        maxLevel = STEPS_PER_LEVEL*current;
      }
              
      for (int lstep = 0; lstep < STEPS_PER_LEVEL; lstep++) {
        // iterate over LED half length
        int led_power;
        for (int dot = 0; dot < NUM_LEDS; dot++) {
          led_power = base_level + shift_vals[dot]*shift_direction;
          if (led_power > maxLevel) {
            led_power = maxLevel;
          }
          if (led_power < minLevel) {
            led_power = minLevel;
          }
          set_led(dot, led_power);
        }
        for (int dot = NUM_LEDS+1; dot < NUM_LEDS+LED_GAP; dot++) {
          leds_l[dot].setRGB(0,0,0);
          leds_r[dot].setRGB(0,0,0);
        }
        FastLED.show();
        // increase shift values
        move_shiftvalues();

        delay(STEP_INTERVAL/STEPS_PER_LEVEL);
      }
    }
  }
  return target;
}

