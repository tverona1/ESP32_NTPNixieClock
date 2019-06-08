/*
    ESP32 NTP Nixie Tube Clock Program

    LEDControl.h - LED Control Code
*/

#ifndef LED_CONTROL_H
#define LED_CONTROL_H

// A 24 bit color type
typedef struct {
  byte red;
  byte green;
  byte blue;
}
RGB24;

// Misc color definitions
RGB24 black   = {  0,   0,   0};
RGB24 blue    = {  0,   0, 127};
RGB24 green   = {  0, 127,   0};
RGB24 cyan    = {  0, 127, 127};
RGB24 red     = {127,   0,   0};
RGB24 magenta = {127,   0, 127};
RGB24 yellow  = {127, 127,   0};
RGB24 white   = {127, 127, 127};

#define LED_RED_CHANNEL 1
#define LED_GREEN_CHANNEL 2
#define LED_BLUE_CHANNEL 3

// LEDControl Class Definition
class LEDControl {
  public:
    // Class constructor
    LEDControl(int _redPin, int _greenPin, int _bluePin) {

      // Save incoming pin assignments
      redPin   = _redPin;
      greenPin = _greenPin;
      bluePin  = _bluePin;

      // Set up the output pins for the RGB LED
      pinMode(redPin, OUTPUT);
      pinMode(greenPin, OUTPUT);
      pinMode(bluePin,  OUTPUT);

      // Each channel is set up for 12kHz and 10-bit resolution
      ledcSetup(LED_RED_CHANNEL, 12000, 10);
      ledcSetup(LED_GREEN_CHANNEL, 12000, 10);
      ledcSetup(LED_BLUE_CHANNEL, 12000, 10);

      ledcAttachPin(redPin, LED_RED_CHANNEL);
      ledcAttachPin(greenPin, LED_GREEN_CHANNEL);
      ledcAttachPin(bluePin, LED_BLUE_CHANNEL);

      // Turn off RGB LEDs
      ledcWrite(LED_RED_CHANNEL, 0);
      ledcWrite(LED_GREEN_CHANNEL, 0);
      ledcWrite(LED_BLUE_CHANNEL, 0);
    }

    // Input a value 0 to 255 to get a color value.
    // The colors are a transition r - g - b - back to r.
    RGB24 colorWheel(int wheelPos) {
      RGB24 color;

      wheelPos %= 256;

      if (wheelPos < 85) {
        color.red   = 255 - wheelPos * 3;
        color.green = wheelPos * 3;
        color.blue  = 0;
      }
      else if (wheelPos < 170) {
        wheelPos   -= 85;
        color.red   = 0;
        color.green = 255 - wheelPos * 3;
        color.blue  = wheelPos * 3;
      }
      else    {
        wheelPos   -= 170;
        color.red   = wheelPos * 3;
        color.green = 0;
        color.blue  = 255 - wheelPos * 3;
      }
      return color;
    }

    // Set the RGB LEDs color
    void setLEDColor(byte red, byte green, byte blue) {
      red   = gammaArray[red];
      green = gammaArray[green];
      blue  = gammaArray[blue];

      // Change 8 bits to 10 bits
      int rred = map(red,   0, 255, 0, 1023);
      int rgrn = map(green, 0, 255, 0, 1023);
      int rblu = map(blue,  0, 255, 0, 1023);

      // Use PWM to control LED brightness
      ledcWrite(LED_RED_CHANNEL, rred);
      ledcWrite(LED_GREEN_CHANNEL, rgrn);
      ledcWrite(LED_BLUE_CHANNEL, rblu);
    }

    // Set the RGB LEDs color
    void setLEDColor(RGB24 color) {
      setLEDColor(color.red, color.green, color.blue);
    }

  private:
    // Private data
    int redPin, greenPin, bluePin;

    // Gamma correction array
    const byte gammaArray [256] = {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
      1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
      2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
      5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
      10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
      17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
      25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
      37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
      51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
      69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
      90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
      115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
      144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
      177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
      215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255
    };
};

#endif
