/*
    ESP32 NTP Nixie Tube Clock Program

    NTP.h - Network Time Protocol Functions
*/

#ifndef NTP_H
#define NTP_H

#include <TimeLib.h>
#include <WiFiUdp.h>

// Define the time between sync events
#define SYNC_INTERVAL_HOURS   1
#define SYNC_INTERVAL_MINUTES (SYNC_INTERVAL_HOURS   * 60L)
#define SYNC_INTERVAL_SECONDS (SYNC_INTERVAL_MINUTES * 60L)

#define NTP_SERVER_NAME "time.nist.gov" // NTP request server
#define NTP_SERVER_PORT  123 // NTP requests are to port 123
#define LOCALPORT       2390 // Local port to listen for UDP packets
#define NTP_PACKET_SIZE   48 // NTP time stamp is in the first 48 bytes of the message
#define RETRIES           20 // Times to try getting NTP time before failing

class NTP {
  public:
    NTP(NixieTubeShield& shield) : _shield(shield) {
      // Login succeeded so set UDP local port
      udp.begin(LOCALPORT);
    };

    static NTP& getInstance() {
      return *_instance;
    }

    static void createSingleton(NixieTubeShield& shield) {
      NTP* ntp = new NTP(shield);
      _instance = ntp;
    }
  
    // Get NTP time with retries on access failure
    time_t getTime() {
      unsigned long result;
    
      for (int i = 0; i < RETRIES; i++) {
        result = _getTime();
        if (result != 0) {
          // Update RTC
          tmElements_t tm;
          breakTime(result, tm);
          _shield.setRTCDateTime(tm);
          return result;
        }
        Serial.println("Problem getting NTP time. Retrying...");
        delay(300);
      }
      Serial.println("NTP Problem - Could not obtain time. Falling back to RTC");
    
      return _getRTCTime();
    }
  
  private:
    // Static NTP instance
    static NTP* _instance;
  
    // NTP Time Provider Code
    time_t _getTime() {
      // Set all bytes in the buffer to 0
      memset(packetBuffer, 0, NTP_PACKET_SIZE);
    
      // Initialize values needed to form NTP request
      packetBuffer[0]  = 0xE3;  // LI, Version, Mode
      packetBuffer[2]  = 0x06;  // Polling Interval
      packetBuffer[3]  = 0xEC;  // Peer Clock Precision
      packetBuffer[12] = 0x31;
      packetBuffer[13] = 0x4E;
      packetBuffer[14] = 0x31;
      packetBuffer[15] = 0x34;
    
      // All NTP fields initialized, now send a packet requesting a timestamp
      udp.beginPacket(NTP_SERVER_NAME, NTP_SERVER_PORT);
      udp.write(packetBuffer, NTP_PACKET_SIZE);
      udp.endPacket();
    
      // Wait a second for the response
      delay(1000);
    
      // Listen for the response
      if (udp.parsePacket() == NTP_PACKET_SIZE) {
        udp.read(packetBuffer, NTP_PACKET_SIZE);  // Read packet into the buffer
        unsigned long secsSince1900;
    
        // Convert four bytes starting at location 40 to a long integer
        secsSince1900 =  (unsigned long) packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long) packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long) packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long) packetBuffer[43];
    
        Serial.println("Got NTP time");
    
        return secsSince1900 - 2208988800UL;
      } else  {
        return 0;
      }
    }
    
    // Get system time from real-time clock
    time_t _getRTCTime() {
      bool isRTCAvailable = true;
      tmElements_t m;
      _shield.getRTCTime(m);
    
      byte prevSeconds = m.Second;
      unsigned long RTC_ReadingStartTime = millis();
    
      Serial.print("Real-time clock: ");
      Serial.print(m.Hour);
      Serial.print(":");
      Serial.println(m.Minute);
      Serial.print(":");
      Serial.println(m.Second);
    
      while (prevSeconds == m.Second) {
        _shield.getRTCTime(m);
        if ((millis() - RTC_ReadingStartTime) > 3000) {
          Serial.println("Warning! RTC did not respond!");
          isRTCAvailable = false;
          break;
        }
      }
    
      // Set system time if RTC is available
      if (isRTCAvailable) {
        Serial.println("Got time from RTC");
        return makeTime(m);
      } else {
        return 0;
      }
    }

    // Instance of shield
    NixieTubeShield& _shield;

    // A UDP instance to let us send and receive packets over UDP
    WiFiUDP udp;

    // Buffer to hold outgoing and incoming packets
    byte packetBuffer[NTP_PACKET_SIZE];
};

NTP* NTP::_instance = 0;

time_t getNTPTime() {
  return NTP::getInstance().getTime();
}

// Initialize the NTP code
void initNTP(NixieTubeShield& shield) {
  // Create instance of NTP class
  NTP::createSingleton(shield);

  // Set the time provider to NTP
  setSyncProvider(getNTPTime);

  // Set the interval between NTP calls
  setSyncInterval(SYNC_INTERVAL_SECONDS);
}

#endif
