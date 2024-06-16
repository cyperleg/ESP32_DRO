#include "screen.h"
#include <EEPROM.h>
#include <driver/adc.h>
#include <FS.h>
#include "SPIFFS.h"

//first micrometer
#define CLOCK_PIN_1 26
#define DATA_PIN_1  27

//first micrometer
#define CLOCK_PIN_2 32
#define DATA_PIN_2  33
//TODO: check pins
#define BUZZER_PIN 12

#define TIME_BETWEEN_READ 300

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
int getBit(bool switchero) {
    int data;
    if (switchero){
          
      int readTimeout = millis() + BIT_READ_TIMEOUT;
      while (analogRead(CLOCK_PIN_1) > ADC_TRESHOLD) {
        if (millis() > readTimeout)
          return -1;
      }
      
      while (analogRead(CLOCK_PIN_1) < ADC_TRESHOLD) {
        if (millis() > readTimeout)
          return -1;
      }
      
      data = (analogRead(DATA_PIN_1) > ADC_TRESHOLD)?1:0;
      return data;
    }
    else
    {
          
      int readTimeout = millis() + BIT_READ_TIMEOUT;
      while (analogRead(CLOCK_PIN_2) > ADC_TRESHOLD) {
        if (millis() > readTimeout)
          return -1;
      }
      
      while (analogRead(CLOCK_PIN_2) < ADC_TRESHOLD) {
        if (millis() > readTimeout)
          return -1;
      }
      
      data = (analogRead(DATA_PIN_2) > ADC_TRESHOLD)?1:0;
      return data;
      }
    
}

// read one full packet
long getPacket(bool switchero) 
{
  Serial.println(switchero);
    long packet = 0;
    int readTimeout = millis() + PACKET_READ_TIMEOUT;

    int bitIndex = 0;
    while (bitIndex < PACKET_BITS) {
      int bit = getBit(switchero);
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

int mostFrequent(int index) {
    int maxFreq = 0;
    int mostFreqElement = dro_buffer[index].microns; // начинаем с последнего элемента

    // Проходим по последним пяти элементам и подсчитываем количество каждого значения
    for (int i = index - 1; i >= 0 && i >= index - 5; --i) {
        int currentElement = dro_buffer[i].microns;
        int freq = 0; // Счетчик частоты текущего элемента
        for (int j = index - 1; j >= 0 && j >= index - 5; --j) {
            if (dro_buffer[j].microns == currentElement) {
                freq++;
            }
        }
        // Обновляем наиболее часто встречающийся элемент, если необходимо
        if (freq > maxFreq) {
            maxFreq = freq;
            mostFreqElement = currentElement;
        }
    }

    return mostFreqElement;
}
int spcTask(bool switchero)
{
    uint32_t lastReadTime = millis();

    for(;;) { 
      long packet = getPacket(switchero);
      
      if (packet < 0) {
        // read timeout, display?
        if (millis() > lastReadTime + PACKET_READ_TIMEOUT) {
          // advance last read to time-out
          lastReadTime = millis();
          Serial.print(lastReadTime);
          Serial.print(" ");
          Serial.print(switchero);
          Serial.println();
        }
      } else {

        // add to local queue
        size_t new_dro_index = (dro_index+1) % DRO_BUFFER_SIZE;
        dro_buffer[new_dro_index].timestamp = millis();
        int res = getMicrons(packet);
        dro_buffer[new_dro_index].microns = res;
        dro_index = new_dro_index;

        // broadcast to all websocket clients
        Serial.print(res);
        Serial.print(" ");
        Serial.println(switchero);
        //res = mostFrequent(dro_index);
        
        //if (res > 1700 || res < -10) res = spcTask(switchero);
        //else return res;
        return res;
      }
    } 
}

void setup() {
  Reading start_reading;
  dro_buffer[dro_index] = start_reading;

  Serial.begin(115200);
  Serial.println("EspDRO initialized.");

  analogReadResolution(11);
  
  analogSetAttenuation(ADC_6db);
  adc1_config_width(ADC_WIDTH_BIT_10);

  pinMode(CLOCK_PIN_1, INPUT);
  pinMode(DATA_PIN_1, INPUT);
  pinMode(CLOCK_PIN_2, INPUT);
  pinMode(DATA_PIN_2, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  configure();
  set_axis();
  //get_screen();
}

float result1, result2 = 0;

void loop() {
  result1 = spcTask(true) / 100.0;
  result2 = spcTask(false) / 100.0;

  if ((result1 * 100 <= VALUE_LOW || result1 * 100 >= VALUE_HIGH) || (result2 * 100 <= VALUE_LOW || result2 * 100 >= VALUE_HIGH)) digitalWrite(BUZZER_PIN, 1);
  else digitalWrite(BUZZER_PIN, 0);

  add_text_value(result1, result2);

  add_value(result1, result2);
  set_graph();

  delay(300);
}
