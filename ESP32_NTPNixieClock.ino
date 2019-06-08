/*
    ESP32 NTP Nixie Tube Clock Program

    This hardware/software combination implements a Nixie tube digital clock that
    never needs setting as it gets the current time and date by polling
    Network Time Protocol (NTP) servers on the Internet. The clock's time
    is synchronized to NTP time periodically. Use of the TimeZone
    library means that Daylight Savings Time (DST) is automatically
    taken into consideration so no time change buttons are necessary.
    Clock can run in 24 hour or 12 hour mode.

    This program uses the WiFiManager library to allow WiFi credentials to be set
    via a web interface.

    How it works:

    When the program is first started it creates a wireless access point called NixieClock
    that the user needs to connect to and then if the user points his/her browser to
    192.168.4.1, a page is presented that allows the credentials for the actual WiFi network
    to be entered. This only needs to be done once since the credentials will be stored in
    EEPROM and will be used from that point forward.

    Once WiFi connection has been established, it uses NTP to initialize the real-time clock (RTC).
    If ESP32 cannot connect to the WiFi network using the stored crecentials, it will fall back
    to the real-time clock (RTC).

    Press the mode button to enter WiFi AP configuration mode.
    Long-press the mode button to reset the ESP32.

    The hardware consists of the following parts:
      ESP32
      Nixie Tubes Clock Arduino Shield NCS314 for xUSSR IN-14 Nixie Tubes from eBay

    The connections between the ESP32 board and the Nixie Clock Shield are as follows:

    ESP32 Pin    Shield Pin          Function
    --------------------------------------------------------------------
        GIOP18        SCK        SPI Clock
        GIOP23        MOSI       Master Out Slave In (data from ESP32->clock)
        SS            LE         Latch Enable
        A18           DOT1       Neon dot 1
        A18           DOT2       Neon dot 2
        A16           PWM1       Green LED drive
        A14           PWM2       Red LED drive
        A13           PWM3       Blue LED drive
        17            SHTDN      High voltage enable
        A15           SET        Set (mode) button
        16            UP         Up button
        A10           DOWN       Down button
        A4            BUZZER     Buzzer pin

    The ESP32 is powered (via Vin) with 5VDC from a 5 volt regulator driven from
    the 12 VDC supply. The shield is powered directly from the 12 VDC supply.

    Based on ESP8266_NTPNixieClock by Craig A. Lindley and NixeTubesShieldNCS14 by GRA & AFCH.

    Copyright (c) 2019 Tomer Verona

    Permission is hereby granted, free of charge, to any person obtaining a copy of
    this software and associated documentation files (the "Software"), to deal in
    the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    the Software, and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
    COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <driver/dac.h>
#include <WiFi.h>
#include <SPI.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>

#include "LEDControl.h"
#include "NixieTubeShield.h"
#include "NTP.h"

// ***************************************************************
// Start of user configuration items
// ***************************************************************

// Name of AP when configuring WiFi credentials
#define AP_NAME "NixieClock"

// Checks WiFi connection. Reset after this time, if WiFi is not connected
#define WIFI_CHK_TIME_SEC 3600
#define WIFI_CHK_TIME_MS  (WIFI_CHK_TIME_SEC * 1000)

// Set to false for 24 hour time mode
#define HOUR_FORMAT_12 true

// Nixie tubes are turned off at night to increase their lifetime
// Clock off and on times are in 24 hour format
#define CLOCK_OFF_HOUR 23
#define CLOCK_ON_HOUR  07

// Suppress leading zeros
// Set to false to having leading zeros displayed
#define SUPPRESS_LEADING_ZEROS true

// Define the timezone in which the clock will operate
// See the Timezone library for details
// US Pacific Time Zone (Las Vegas, Los Angeles)
TimeChangeRule usPDT = {"PDT", Second, Sun, Mar, 2, -420};
TimeChangeRule usPST = {"PST", First, Sun, Nov, 2, -480};
Timezone TZ(usPDT, usPST);

// ***************************************************************
// End of user configuration items
// ***************************************************************

// Instantiate the Nixie Tube Shield object
NixieTubeShield SHIELD;

// Instantiate the WifiManager object
WiFiManager wifiManager;

// ***************************************************************
// Utility Functions
// ***************************************************************

// Misc variables
int previousMinute = 0;
int minutes = 0;
boolean dotToggle = false;
boolean clockOn = true;

// This function is called once a second
void updateDisplay(void) {

  // Get the current time and date
  // Get the time for specified timezone
  time_t localTime = TZ.toLocal(now());

  // Determine if clock should be on or off
  int hr = hour(localTime);

  // Clock is on between these hours
  if ((hr >= CLOCK_ON_HOUR) && (hr < CLOCK_OFF_HOUR)) {
    if (!clockOn) {
      clockOn = true;
      Serial.println("Clock is On");

      // Clock should be on so turn high voltage on
      SHIELD.hvEnable(true);
    }
  } else  {
    if (clockOn) {
      clockOn = false;

      Serial.println("Clock is Off");

      // Clock is going off so shut things down in an orderly fashion
      // First turn off the high voltage so the nixie's go off
      SHIELD.hvEnable(false);

      // Next turn off the dots
      SHIELD.dotsEnable(false);
      SHIELD.setNX1Digit(BLANK_DIGIT);
      SHIELD.setNX2Digit(BLANK_DIGIT);
      SHIELD.setNX3Digit(BLANK_DIGIT);
      SHIELD.setNX4Digit(BLANK_DIGIT);
      SHIELD.setNX5Digit(BLANK_DIGIT);
      SHIELD.setNX6Digit(BLANK_DIGIT);
      SHIELD.show();

      // Finally turn the LEDs off as well
      SHIELD.setLEDColor(black);
    }

    // No need to continue as the clock is effectively off
    return;
  }

  // Get the current minute
  minutes = minute(localTime);

  // Dispatch events in priority order

  // 60 minute event is anti-poisoning routine
  if ((minutes != previousMinute) && (minutes == 0)) {
    previousMinute = minutes;

    // Set all LEDs to red to indicate anti-poisoning
    SHIELD.setLEDColor(red);

    // Do anti-poisoning function
    SHIELD.doAntiPoisoning();
  }

  // 15 minute event is rainbow display
  else if ((minutes != previousMinute) && (minutes != 0) && ((minutes % 15) == 0)) {
    previousMinute = minutes;

    // Turn off dots
    SHIELD.dotsEnable(false);

    // Blank all nixie tube digits
    SHIELD.setNX1Digit(BLANK_DIGIT);
    SHIELD.setNX2Digit(BLANK_DIGIT);
    SHIELD.setNX3Digit(BLANK_DIGIT);
    SHIELD.setNX4Digit(BLANK_DIGIT);
    SHIELD.setNX5Digit(BLANK_DIGIT);
    SHIELD.setNX6Digit(BLANK_DIGIT);

    // Blank the nixie tubes
    SHIELD.show();

    // Cycle the LEDs through rainbows of color
    for (int cycles = 0; cycles < 4; cycles++) {
      for (int cycle = 0; cycle < 256; cycle += 16) {
        // Set the LED's color
        SHIELD.setLEDColor(SHIELD.colorWheel(cycle));
        delay(400);
      }
    }
  }

  // 10 minute event is the date display
  else if ((minutes != previousMinute) && (minutes != 0) && ((minutes % 10) == 0)) {
    previousMinute = minutes;

    // Turn off dots
    SHIELD.dotsEnable(false);

    // Set all LEDs to blue to indicate date display
    SHIELD.setLEDColor(blue);

    SHIELD.dotsEnable(true);

    // Get the current month 1..12
    int now_mon  = month(localTime);

    // Display the NX1 digit
    if (now_mon >= 10) {
      SHIELD.setNX1Digit(now_mon / 10);
    } else  {
      if (SUPPRESS_LEADING_ZEROS) {
        SHIELD.setNX1Digit(BLANK_DIGIT);
      } else  {
        SHIELD.setNX1Digit(0);
      }
    }
    // Display the NX2 digit
    SHIELD.setNX2Digit(now_mon % 10);

    // Get the current day 1..31
    int now_day  = day(localTime);

    // Display the NX3 digit
    if (now_day >= 10) {
      SHIELD.setNX3Digit(now_day / 10);
    } else  {
      if (SUPPRESS_LEADING_ZEROS) {
        SHIELD.setNX3Digit(BLANK_DIGIT);
      } else  {
        SHIELD.setNX3Digit(0);
      }
    }
    // Display the NX4 digit
    SHIELD.setNX4Digit(now_day % 10);

    // Get the current year
    int now_year = year(localTime) - 2000;

    // Display the NX5 digit
    if (now_year >= 10) {
      SHIELD.setNX5Digit(now_year / 10);
    } else  {
      if (SUPPRESS_LEADING_ZEROS) {
        SHIELD.setNX5Digit(BLANK_DIGIT);
      } else  {
        SHIELD.setNX5Digit(0);
      }
    }
    // Display the NX6 digit
    SHIELD.setNX6Digit(now_year % 10);

    // Display date on clock
    SHIELD.show();

    // Delay for date display
    delay(10000);

  } else {
    // Display the time

    int now_hour;
    float colorInc;

    // Make the dots blink off and on
    if (dotToggle) {
      dotToggle = false;
      SHIELD.dotsEnable(true);

    } else  {
      dotToggle = true;
      SHIELD.dotsEnable(false);
    }

    // Set the LED's color depending upon the hour
    // Get the current hour
    if (HOUR_FORMAT_12) {
      // Using 12 hour format
      now_hour = hourFormat12(localTime);
      // Calculate the color of the LEDs based on the hour in 12 hour format
      colorInc = 256 / 12.0;
    } else  {
      // Using 24 hour format
      now_hour = hour(localTime);
      // Calculate the color of the LEDs based on the hour in 24 hour format
      colorInc = 256 / 24.0;
    }
    // Set the shields's LED color
    SHIELD.setLEDColor(SHIELD.colorWheel(colorInc * now_hour));

    // Display the NX1 digit
    if (now_hour >= 10) {
      SHIELD.setNX1Digit(now_hour / 10);
    } else  {
      if (SUPPRESS_LEADING_ZEROS) {
        SHIELD.setNX1Digit(BLANK_DIGIT);
      } else  {
        SHIELD.setNX1Digit(0);
      }
    }
    // Display the NX2 digit
    SHIELD.setNX2Digit(now_hour % 10);

    // Get the current minute
    int now_min  = minute(localTime);

    // Display the NX3 digit
    SHIELD.setNX3Digit(now_min / 10);

    // Display the NX4 digit
    SHIELD.setNX4Digit(now_min % 10);

    // Get the current second
    int now_sec  = second(localTime);

    // Display the NX5 digit
    SHIELD.setNX5Digit(now_sec / 10);

    // Display the NX6 digit
    SHIELD.setNX6Digit(now_sec % 10);

    // Display time on clock
    SHIELD.show();
  }
}

// Get WiFi SSID
String WIFI_GetSSID() {
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);
  return String(reinterpret_cast<const char*>(conf.sta.ssid));
}

// Get WiFi Password
String WIFI_GetPassword() {
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);
  return String(reinterpret_cast<const char*>(conf.sta.password));
}

// Attempt to connect to WiFi
void WIFI_Connect() {
  WiFi.disconnect();
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_GetSSID().c_str(), WIFI_GetPassword().c_str());

  for (int i = 0; i < 40; i++) {
    if (WiFi.status() != WL_CONNECTED) {
      delay (250);
      SHIELD.setLEDColor(black);
      Serial.print(".");
      delay (250);
      SHIELD.setLEDColor(green);
    } else {
      break;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi Connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wifi NOT connected");
  }
}

void WIFI_StartAccessPoint() {
  // Starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  Serial.println("Starting Wifi AP");
  WiFiManager wifiManager;
  wifiManager.startConfigPortal(AP_NAME);
}

// ***************************************************************
// Program Setup
// ***************************************************************

void setup() {
  // Turn off the high voltage for the clock
  SHIELD.hvEnable(false);

  // Turn off DAC channels
  dac_output_disable(DAC_CHANNEL_1);
  dac_output_disable(DAC_CHANNEL_2);
  dac_i2s_disable();

  Wire.begin();

  // Configure serial interface
  Serial.begin(115200);
  delay(1000);
  Serial.println();

  pinMode(SS, OUTPUT);

  // Setup SPI interface to defaults for shield
  SPI.begin();
  SPI.setDataMode (SPI_MODE2);  // Mode 2 SPI
  SPI.setClockDivider(2000000); // SCK = 2MHz

  // Reset saved settings for testing purposes
  // Should be commented out for normal operation
  // wifiManager.resetSettings();

  WiFi.mode(WIFI_AP_STA);
  Serial.println(WIFI_GetSSID());
  Serial.println(WIFI_GetPassword());

  // If WiFi setup is not configured, start access point
  // Otherwise, connect to WiFi
  if (0 == WIFI_GetSSID().length()) {
    WIFI_StartAccessPoint();
  } else {
    WIFI_Connect();
  }
  delay(100);

  initNTP(SHIELD);

  // Set all LEDs to black or off
  SHIELD.setLEDColor(black);

  // Turn off the dots
  SHIELD.dotsEnable(false);

  // Turn on the high voltage for the clock
  SHIELD.hvEnable(true);

  // Do the anti poisoning routine
  SHIELD.doAntiPoisoning();

  Serial.println("\nReady!\n");
}

// ***************************************************************
// Program Loop
// ***************************************************************

unsigned long nextConnectionCheckTime = 0;
int previousSecond = 0;

void loop() {
  // Check WiFi connectivity
  if (millis() > nextConnectionCheckTime) {
    Serial.print("\n\nChecking WiFi... ");
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost. Reconnecting...");
      WIFI_Connect();
    } else {
      Serial.println("Wifi connected");
    }
    nextConnectionCheckTime = millis() + WIFI_CHK_TIME_MS;
  }

  // Process button status
  SHIELD.processButtons();

  // If set button is pressed, enter Wifi AP mode
  if (SHIELD.isSetButtonClicked()) {
    WIFI_StartAccessPoint();
  }

  // If set button is long-pressed, restart ESP
  if (SHIELD.isSetButtonLongClicked()) {
    esp_restart();
  }

  // Update the display only if time has changed
  if (timeStatus() != timeNotSet) {
    if (second() != previousSecond) {
      previousSecond = second();

      // Display updated once a second
      updateDisplay();
    }
  }
  delay(10);
}
