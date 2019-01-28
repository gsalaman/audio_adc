// Sampling for mega.
// Second cut...time visualizer, full screen. 

#include <Adafruit_GFX.h>   // Core graphics library
#include <RGBmatrixPanel.h> // Hardware-specific library

// Pin defines for the 32x32 RGB matrix.
#define CLK 11  
#define LAT 10
#define OE  9
#define A   A0
#define B   A1
#define C   A2
#define D   A3

/* Other Pin Mappings...hidden in the RGB library:
 *  Sig   Uno  Mega
 *  R0    2    24
 *  G0    3    25
 *  B0    4    26
 *  R1    5    27
 *  G1    6    28
 *  B1    7    29
 */

// Note "false" for double-buffering to consume less memory, or "true" for double-buffered.
RGBmatrixPanel matrix(A, B, C,  D,  CLK, LAT, OE, true);

//  We're using A5 as our audio input pin.
//  To use the (slower) analogRead(), comment out this define.  Otherwise we do a faster bitbang of the ADC.
//  NOTE:  If you are doing bit-banging, you need to connect 5v ref to AREF pin on the mega.
//#define BIT_BANG_ADC

#define MUX_MASK 0x05
#define AUDIO_PIN A5

#define GAIN_PIN  A4
int gain=1;

// These are the raw samples from the audio input.
#define SAMPLE_SIZE 32
int sample[SAMPLE_SIZE] = {0};

//  Audio samples from the ADC are "centered" around 2.5v, which maps to 512 on the ADC.
#define SAMPLE_BIAS 512

// Mapped sample should give a number between 0 and 31.
int map_sample( int input )
{
  int mapped_sample;

  // start by taking out DC bias.  This will make negative #s...
  mapped_sample = input - SAMPLE_BIAS;

  // add in gain.
  mapped_sample = mapped_sample / gain;
  
  // center on 16.
  mapped_sample = mapped_sample + 16;

  // and clip.
  if (mapped_sample > 31) mapped_sample = 31;
  if (mapped_sample < 0) mapped_sample = 0;

  return mapped_sample;
}

void read_gain( void )
{
   int raw_gain;

   raw_gain = analogRead(GAIN_PIN);
   gain = map(raw_gain, 0, 1023, 1, 32);
}


void show_samples_lines( void )
{
  int x;
  int y;
  int last_x=0;
  int last_y=16;

  matrix.fillScreen(0);
  
  for (x=0; x < SAMPLE_SIZE; x++)
  {
    y=map_sample(sample[x]);
    matrix.drawLine(last_x,last_y,x,y,matrix.Color333(0,0,1));
    last_x = x;
    last_y = y;
  }
}

void setup() 
{
  Serial.begin(9600);

  matrix.begin();
  
  #ifdef BIT_BANG_ADC
  setupADC();
  #endif
}

void setupADC( void )
{

    ADCSRA = 0b11100101;      // set ADC to free running mode and set pre-scalar to 32 (0xe5)
                              // pre-scalar 32 should give sample frequency of 38.64 KHz...which
                              // will reproduce samples up to 19.32 KHz

   // MATH FROM ABOVE...in measurements, it looks like prescalar of 32 gives me sample freq of 40 KHz
   //    on the UNO.

    // A5, internal reference.
    ADMUX =  MUX_MASK;
    
    delay(50);  //wait for voltages to stabalize.  
}

void collect_accurate_samples( void )
{
  //use ADC internals.

  int i;

  for (i=0; i<SAMPLE_SIZE; i++)
  {
    while(!(ADCSRA & 0x10));        // wait for ADC to complete current conversion ie ADIF bit set
    ADCSRA = 0b11110101 ;               // clear ADIF bit so that ADC can do next operation (0xf5)

    sample[i] = ADC;
  }

  
  
}

void collect_samples( void )
{
  int i;
  for (i = 0; i < SAMPLE_SIZE; i++)
  {
    sample[i] = analogRead(AUDIO_PIN);
  }
}



void print_samples( void )
{
  int i;

  Serial.println("Sample Buffer: ");
  for (i=0; i<SAMPLE_SIZE; i++)
  {
    Serial.println(sample[i]);
  }
  Serial.println("===================");
}


void loop() 
{

  read_gain();
  
  #ifdef BIT_BANG_ADC
  collect_accurate_samples();
  #else
  collect_samples();
  #endif

  show_samples_lines();

  matrix.swapBuffers(true);
  
}
