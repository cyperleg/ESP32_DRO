#include <EEPROM.h>
#include <driver/adc.h>
#include <FS.h>
#include "SPIFFS.h"


// Note: this sketch requires the webserver files to be uploaded to ESP32 SPIFFS, see: https://github.com/me-no-dev/arduino-esp32fs-plugin

// Pin Configuration - !!! Update This !!! 
// Example pinout: https://raw.githubusercontent.com/gojimmypi/ESP32/master/images/myESP32%20DevKitC%20pinout.png
int dataPin = 27; // yellow
int clockPin = 26; // green

// ADC threshold for 1.5V SPCsignals (at 6dB/11-bit, high comes to around 1570 in analogRead() )
#define ADC_TRESHOLD 800

// define this to display the raw signal (useful to troubleshoot cable or signal level issues)
#define DEBUG_SIGNAL 0

// timeout in milliseconds for a bit read ($TODO - change to micros() )
#define BIT_READ_TIMEOUT 100

// timeout for a packet read 
#define PACKET_READ_TIMEOUT 250

// Packet format: [0 .. 19]=data, 20=sign, [21..22]=unused?, 23=inch
#define PACKET_BITS       24

// minimum reading
#define MIN_RANGE -(1<<20)

// DRO circular buffer entries (4096 entries is roughly the equivalent of 8 minutes of data)
#define DRO_BUFFER_SIZE  0x1000

#define ESPDRO_VERSION "0.99.1"

void log(char *fmt, ... )
{  
  char buf[128];
  va_list args;
  va_start (args, fmt);
  vsnprintf(buf, 128, fmt, args);
  va_end (args);
  Serial.println(buf);
}

// DRO 1-Reading class
struct Reading
{  
    uint32_t timestamp;
    int32_t microns;

    Reading(): timestamp(millis()), microns(MIN_RANGE)  {}

    Reading& operator=(const Reading& obj) {
      timestamp = obj.timestamp;
      microns = obj.microns;
      return *this;
    }    
};


// DRO circular buffer
Reading dro_buffer[DRO_BUFFER_SIZE] = {};
size_t dro_index = 0;

unsigned char eepromSignature = 0x5A;

volatile bool debug_mode = 0;

// capped read: -1 (timeout), 0, 1
int getBit() {
    int data;

    if (debug_mode) {
      // debug code to sample the data reads
      log("CLK:%6d DATA:%6d\n", analogRead(clockPin), analogRead(dataPin));
    }
        
    int readTimeout = millis() + BIT_READ_TIMEOUT;
    while (analogRead(clockPin) > ADC_TRESHOLD) {
      if (millis() > readTimeout)
        return -1;
    }
    
    while (analogRead(clockPin) < ADC_TRESHOLD) {
      if (millis() > readTimeout)
        return -1;
    }
    
    data = (analogRead(dataPin) > ADC_TRESHOLD)?1:0;
    return data;
}

// read one full packet
long getPacket() 
{
    long packet = 0;
    int readTimeout = millis() + PACKET_READ_TIMEOUT;

    int bitIndex = 0;
    while (bitIndex < PACKET_BITS) {
      int bit = getBit();
      if (bit < 0 ) {
        // bit read timeout: reset packet or bail out
        if (millis() > readTimeout) {
          // packet timeout
          return -1;
        }
        
        bitIndex = 0;
        packet = 0;
        continue;
      }
 
      packet |= (bit & 1)<<bitIndex;
      
      bitIndex++;
    }
    
    return packet;
}

// convert a packet to signed microns
long getMicrons(long packet)
{
  if (packet < 0)
    return MIN_RANGE;
    
  long data = (packet & 0xFFFFF)*( (packet & 0x100000)?-1:1);

  if (packet & 0x800000) {
        // inch value (this comes sub-sampled) 
        data = data*254/200;
  }

  return data;
}


void spcTask()
{
    uint32_t lastReadTime = millis();

    for(;;) { 
      long packet = getPacket();
      
      if (packet < 0) {
        // read timeout, display?
        if (millis() > lastReadTime + PACKET_READ_TIMEOUT) {
          // advance last read to time-out
          lastReadTime = millis();
          log("* %d: no SPC data", lastReadTime);
        }
      } else {

        // add to local queue
        //log("* %d: microns=%d raw=0x%08X", millis(), getMicrons(packet), packet);
        size_t new_dro_index = (dro_index+1) % DRO_BUFFER_SIZE;
        dro_buffer[new_dro_index].timestamp = millis();
        int res = getMicrons(packet);
        dro_buffer[new_dro_index].microns = res;
        dro_index = new_dro_index;

        // broadcast to all websocket clients
        Serial.println(res);
      }
    } 
    //vTaskDelete( NULL ); 
}

void setup()
{
  pinMode(dataPin, INPUT);     
  pinMode(clockPin, INPUT);

  EEPROM.begin(512);
  delay(20);

  Reading start_reading;
  dro_buffer[dro_index] = start_reading;

  Serial.begin(115200);
  log("EspDRO %s initialized.", ESPDRO_VERSION);

  analogReadResolution(11);
  
  analogSetAttenuation(ADC_6db);
  adc1_config_width(ADC_WIDTH_BIT_10);
}


size_t last_dro_index = 0;
void loop()
{
  spcTask();
}