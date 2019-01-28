// Take 2.  Use ADC clock.  Not gonna worry about setting for input bias...I think the mic does
//  that for us.  
#include <Adafruit_GFX.h>   // Core graphics library
#include <RGBmatrixPanel.h> // Hardware-specific library
#include <arduinoFFT.h>


#define CLK 11  // MUST be on PORTB! (Use pin 11 on Mega)
#define LAT 10
#define OE  9
#define A   A0
#define B   A1
#define C   A2
#define D   A3

/* Pin Mapping:
 *  Sig   Uno  Mega
 *  R0    2    24
 *  G0    3    25
 *  B0    4    26
 *  R1    5    27
 *  G1    6    28
 *  B1    7    29
 */

// Last parameter = 'true' enables double-buffering, for flicker-free,
// buttery smooth animation.  Note that NOTHING WILL SHOW ON THE DISPLAY
// until the first call to swapBuffers().  This is normal.
//RGBmatrixPanel matrix(A, B, C,  D,  CLK, LAT, OE, false);
// Double-buffered mode consumes nearly all the RAM available on the
// Arduino Uno -- only a handful of free bytes remain.  Even the
// following string needs to go in PROGMEM:

#define DISPLAY_COLUMNS 32

#define ENVELOPE_PIN A5

// NOTE!!! if doing "collect_accurate_samples", we're bitbanging the ADC config, and won't
// use this define.
#define AUDIO_PIN A0

// These are the raw samples from the audio input.
#define SAMPLE_SIZE 32
int sample[SAMPLE_SIZE] = {0};

// These are used to do the FFT.
double vReal[SAMPLE_SIZE];
double vImag[SAMPLE_SIZE];
arduinoFFT FFT = arduinoFFT();

// This is our FFT output
char data_avgs[DISPLAY_COLUMNS];

int gain=8;

void setup() 
{
  Serial.begin(9600);
  
  //matrix.begin();

  setupADC();

}

void setupADC( void )
{

    ADCSRA = 0b11100101;      // set ADC to free running mode and set pre-scalar to 32 (0xe5)
                              // pre-scalar 32 should give sample frequency of 38.64 KHz...which
                              // will reproduce samples up to 19.32 KHz

   // MATH FROM ABOVE...in measurements, it looks like prescalar of 32 gives me sample freq of 40 KHz
   //    on the UNO.

    // This was the reference...                          
    //ADMUX = 0b00000000;       // use pin A0 and external voltage reference

    // Trying with internal voltage reference to save a wire...
    // WORKED!
    //ADMUX = 0b01000000;


    // now try moving to A5
    ADMUX = 0b01000101;
    delay(50);  //wait for voltages to stabalize.  Necessary?
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

#define SAMPLE_BIAS 512
#define GAIN        8

// Mapped sample should give a number between 0 and 31
int map_sample( int input )
{
  int mapped_sample;
  
  // Looks like our samples are quiet, so I'm gonna start with a very quiet mapping.

  // start by taking out DC bias.  This will make negative #s...
  mapped_sample = input - SAMPLE_BIAS;

  // Now make this a 0-31 number.  

  // add in gain.
  mapped_sample = mapped_sample / gain;
  
  // center on 16.
  mapped_sample = mapped_sample + 16;

  // and clip.
  if (mapped_sample > 31) mapped_sample = 31;
  if (mapped_sample < 0) mapped_sample = 0;

  return mapped_sample;
}

#if 0
void show_samples( void )
{
  int x;
  int y;

  matrix.fillScreen(0);
  
  for (x=0; x < DISPLAY_COLUMNS; x++)
  {
    y=map_sample(sample[x]);
    matrix.drawPixel(x,y,matrix.Color333(0,0,1));
  }
}


void show_samples_lines( void )
{
  int x;
  int y;
  int last_x=0;
  int last_y=16;

  matrix.fillScreen(0);
  
  for (x=0; x < DISPLAY_COLUMNS; x++)
  {
    y=map_sample(sample[x]);
    matrix.drawLine(last_x,last_y,x,y,matrix.Color333(0,0,1));
    last_x = x;
    last_y = y;
  }
}
#endif

void doFFT( void )
{
  int i;
  int temp_sample;

  for (i=0; i < SAMPLE_SIZE; i++)
  {
    // Remove 512 DC bias
    temp_sample = sample[i] - SAMPLE_BIAS;

    // Load the sample into the complex number...some compression here.
    vReal[i] = temp_sample/8;
    vImag[i] = 0;
  }
  
  FFT.Windowing(vReal, SAMPLE_SIZE, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLE_SIZE, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLE_SIZE);
}

#if 0
void display_freq_raw( void )
{
  int i;
  int mag;

  matrix.fillScreen(0);
  
  for (i = 0; i < SAMPLE_SIZE; i++)
  {
    mag = constrain(vReal[i], 0, 40);
    mag = map(mag, 0, 40, 31, 0);

    matrix.drawLine(i,31,i,mag, matrix.Color333(1,0,0));
  }
  
}
#endif

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
  unsigned long start_time;
  unsigned long stop_time;
  unsigned long delta_t;
  unsigned long per_sample;

  start_time = micros();
  collect_accurate_samples();
  //collect_samples();
  stop_time = micros();
  
  print_samples();

  delta_t = stop_time - start_time;
  per_sample = delta_t / SAMPLE_SIZE;
  
  Serial.print("Time to collect samples (us): ");
  Serial.println( (stop_time - start_time ) );
  Serial.print(per_sample);
  Serial.print(" us per sample (");
  Serial.print( 1000000 / per_sample);
  Serial.println(" Hz)");

  Serial.println("hit enter for next sampling");
  while (!Serial.available());
  while (Serial.available()) Serial.read();
  
}
