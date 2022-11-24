/**********************************************************
 *  INCLUDES
 *********************************************************/
#include "let_it_be_1bit.h"

/**********************************************************
 *  CONSTANTS
 *********************************************************/
// COMMENT THIS LINE TO EXECUTE WITH THE PC
#define TEST_MODE 1

#define SAMPLE_TIME 250 
#define SOUND_PIN  11
#define BUF_SIZE 256

/**********************************************************
 *  GLOBALS
 *********************************************************/
unsigned char buffer[BUF_SIZE];
unsigned long timeOrig;

int muted = false;
int pulsed = false;
int ciclo_led = 0;

/**********************************************************
 * Function: play_bit
 *********************************************************/
void play_bit()
{
  static int bitwise = 1;
  static unsigned char data = 0;
  static int music_count = 0;

  bitwise = (bitwise * 2);
  if (bitwise > 128) {
     bitwise = 1;
     #ifdef TEST_MODE 
        data = pgm_read_byte_near(music + music_count);
        music_count = (music_count + 1) % MUSIC_LEN;
     #else 
        if (Serial.available()>1) {
           data = Serial.read();
        }
     #endif
  }
  digitalWrite(SOUND_PIN, (data & bitwise) );
}

void muteLed() {
  if (!(ciclo_led >= 100000)) return;
  
  bool interruptor = digitalRead(7);
  if (interruptor && !pulsed) {
    pulsed = true;
    muted = !muted;
  } else if (!interruptor && pulsed) {
    pulsed = false; 
  }

  digitalWrite(13, muted);
}

/**********************************************************
 * Function: setup
 *********************************************************/
void setup ()
{
    // Initialize serial communications
    Serial.begin(115200);

    pinMode(SOUND_PIN, OUTPUT);
    pinMode(7, INPUT);
    pinMode(11, OUTPUT);
    pinMode(13, OUTPUT);
    memset (buffer, 0, BUF_SIZE);
    timeOrig = micros(); 
}

/**********************************************************
 * Function: loop
 *********************************************************/
void loop ()
{
    unsigned long timeDiff;

    play_bit();
    timeDiff = SAMPLE_TIME - (micros() - timeOrig);
    timeOrig = timeOrig + SAMPLE_TIME;
    ciclo_led += SAMPLE_TIME;
    delayMicroseconds(timeDiff);
}
