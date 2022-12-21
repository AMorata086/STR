/**********************************************************
 *  INCLUDES
 *********************************************************/
#include "let_it_be_1bit.h"

/**********************************************************
 *  CONSTANTS
 *********************************************************/
// COMMENT THIS LINE TO EXECUTE WITH THE PC
#define TEST_MODE 1

#define SAMPLE_TIME 250 // microseconds
#define BUTTON_PIN 7
#define SOUND_PIN 11
#define LED_MUTE_PIN 13
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
 * Function: setup
 *********************************************************/
void setup()
{
  // Initialize serial communications
  // Serial.begin(115200);
  Serial.begin(9600);

  // pin configuration
  pinMode(SOUND_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(SOUND_PIN, OUTPUT);
  pinMode(LED_MUTE_PIN, OUTPUT);
  memset(buffer, 0, BUF_SIZE);

  // Timer configuration
  /*
   * Toggle on compare match
   * Pre-scaling to 8
   * CTC mode
   */
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;

  OCR1A = 499;
  TCCR1A = _BV(COM1A0);
  TCCR1B = _BV(WGM12) | _BV(CS11);

  TIMSK1 = _BV(OCIE1A);
  interrupts();

  timeOrig = micros();
}

/**********************************************************
 * Function: play_bit
 *********************************************************/

void play_bit()
{
  static int bitwise = 1;
  static unsigned char data = 0;
  static int music_count = 0;

  bitwise = (bitwise * 2);
  if (bitwise > 128)
  {
    bitwise = 1;
#ifdef TEST_MODE
    data = pgm_read_byte_near(music + music_count);
    music_count = (music_count + 1) % MUSIC_LEN;
#else
    if (Serial.available() > 1)
    {
      data = Serial.read();
    }
#endif
  }
  if (!muted)
  {
    digitalWrite(SOUND_PIN, (data & bitwise));
  }
  else
  {
    digitalWrite(SOUND_PIN, 0);
  }
}

/**********************************************************
 * Function: muteLed
 *********************************************************/
void muteLed()
{
  bool interruptor = digitalRead(BUTTON_PIN);
  if (interruptor && !pulsed)
  {
    pulsed = true;
    muted = !muted;
  }
  else if (!interruptor && pulsed)
  {
    pulsed = false;
  }

  digitalWrite(LED_MUTE_PIN, muted);
}

ISR(TIMER1_COMPA_vect)
{
  play_bit();
}

/**********************************************************
 * Function: loop
 *********************************************************/
unsigned long timeDiff;
void loop()
{
  muteLed();

  // calcula tiempo restante y se espera hasta siguiente sampleo
  timeDiff = SAMPLE_TIME - (micros() - timeOrig);
  timeOrig = timeOrig + SAMPLE_TIME;
  delayMicroseconds(timeDiff);
}
