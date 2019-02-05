// Sampling for mega.
// Sample at 10 KHz.  Use bit-bang ADC (needs aref)...so also use external jack.
// Fixed gains.  Volume bar up top, real time middle, spectrum bottom.

// 10 KHz sample gives 5 KHz span for display...divide by 16 to make 312.5 KHz per bin.

#include <Adafruit_GFX.h>   // Core graphics library
#include <RGBmatrixPanel.h> // Hardware-specific library
#include <arduinoFFT.h>

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
#define BIT_BANG_ADC

#define MUX_MASK 0x05
#define AUDIO_PIN A5

#define GAIN_PIN  A4
int gain=8;

// These are the raw samples from the audio input.
#define SAMPLE_SIZE 32
int sample[SAMPLE_SIZE] = {0};

//  Audio samples from the ADC are "centered" around 2.5v, which maps to 512 on the ADC.
#define SAMPLE_BIAS 512

// These are used to do the FFT.
double vReal[SAMPLE_SIZE];
double vImag[SAMPLE_SIZE];
arduinoFFT FFT = arduinoFFT();

// Color pallete for spectrum...cooler than just single green.
uint16_t spectrum_colors[] = 
{
  matrix.Color444(7,0,0),   // index 0
  matrix.Color444(6,1,0),   // index 1
  matrix.Color444(5,2,0),   // index 2
  matrix.Color444(4,3,0),   // index 3
  matrix.Color444(3,4,0),   // index 4
  matrix.Color444(2,5,0),   // index 5
  matrix.Color444(1,6,0),   // index 6
  matrix.Color444(0,7,0),   // index 7 
  matrix.Color444(0,6,1),   // index 8
  matrix.Color444(0,5,2),   // index 9
  matrix.Color444(0,4,3),   // index 10
  matrix.Color444(0,3,4),   // index 11
  matrix.Color444(0,2,5),   // index 12 
  matrix.Color444(0,1,6),   // index 13
  matrix.Color444(0,0,6),   // index 14
  matrix.Color444(0,0,10)    // index 15 
};

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
   // MATH ...in measurements, it looks like prescalar of 32 gives me sample freq of 40 KHz
   //    on the UNO.  Same on mega.  Hmmm.

   
   // Prescalar of 128 gives 9 KHz sample rate.
   // Prescalar of 64 gives 14 KHz.  
   // Prescalar of 32 gives 22 KHz
   // prescalar of 16 gives 52-66.
    
    ADCSRA = 0b11100111;      // set ADC to free running mode and set pre-scalar to 32 (0xe5)
                              // pre-scalar 32 should give sample frequency of 38.64 KHz...which
                              // will reproduce samples up to 19.32 KHz



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
    ADCSRA = ADCSRA | 0x10 ;        // clear ADIF bit so that ADC can do next operation (0xf5)

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

int find_max_amp( void )
{
  int i;
  int max_amp=0;
  int temp_sample;

  for (i=0; i<SAMPLE_SIZE; i++)
  {
    // first need to adjust the samples for the 512 bias.
    temp_sample = sample[i] - 512;

    // then we want the absolute value
    if (temp_sample < 0) temp_sample = 0 - temp_sample;

    // is it bigger than our current max?
    if (temp_sample > max_amp) max_amp = temp_sample;
   }

   return max_amp;
}

void display_amp_bar( void )
{
  int max_amp = find_max_amp();

  // our time visualizer took a voltage from 0 to 1023 and mapped it to 0 to 31.  
  // I want that same type of range...but max_amp is *ONLY* a positive # from 0-511.

  // multiply by 2 (since we're abs, not full +/- range
  max_amp = max_amp * 2;
  
  // add in gain.  
  max_amp = max_amp / gain;

  // clip
  if (max_amp > 31) max_amp = 31;

  matrix.fillRect(0,0,max_amp, 4, matrix.Color333(1,0,0));
}

void doFFT( void )
{
  int i;
  int temp_sample;
  
  for (i=0; i < SAMPLE_SIZE; i++)
  {
    // Remove DC bias
    temp_sample = sample[i] - SAMPLE_BIAS;

    // Load the sample into the complex number...some compression here.
    vReal[i] = temp_sample/4;
    //vReal[i] = temp_sample;
    vImag[i] = 0;
    
  }
  
  FFT.Windowing(vReal, SAMPLE_SIZE, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLE_SIZE, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLE_SIZE);
}

#define MAX_FREQ_MAG 100
void display_freq_raw( void )
{
  int i;
  int mag;
  int x;
  
  for (i = 0; i < SAMPLE_SIZE/2; i++)
  {
    mag = constrain(vReal[i], 0, MAX_FREQ_MAG);
    mag = map(mag, 0, MAX_FREQ_MAG, 0, -8);

    x = 2*i;
    
    matrix.drawRect(x,32,2,mag, spectrum_colors[i]);
  }
}


void loop() 
{

  //read_gain();
  
  #ifdef BIT_BANG_ADC
  collect_accurate_samples();
  #else
  collect_samples();
  #endif

  //print_samples();

  doFFT();
  
  matrix.fillScreen(0);
  display_freq_raw();
  display_amp_bar();
  show_samples_lines();

  matrix.swapBuffers(true);

#if 0
  Serial.println("hit enter for next sampling");
  while (!Serial.available());
  while (Serial.available()) Serial.read();
#endif
  
}
