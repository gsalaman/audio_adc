// 64 sample FHT 

// These two defines are for the RGB Matrix
#include <Adafruit_GFX.h>   // Core graphics library
#include <RGBmatrixPanel.h> // Hardware-specific library


// FHT defines.  This library defines an input buffer for us called fht_input of signed integers.  
// We need to define how that library is used before including it.
#define LIN_OUT 1
#define FHT_N   64
#include <FHT.h>

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
// Double-buffered makes updates look smoother.
RGBmatrixPanel matrix(A, B, C,  D,  CLK, LAT, OE, true);

//  We're using A5 as our audio input pin.
#define AUDIO_PIN A5

// Gain will tell us how to scale the samples to fit in the "time" space display.
// We use this to divide the input signal, so bigger numbers make the input smaller.
int gain=6;

// These are the raw samples from the audio input.
#define SAMPLE_SIZE FHT_N
int sample[SAMPLE_SIZE] = {0};

//  Audio samples from the ADC are "centered" around 2.5v, which maps to 512 on the ADC.
#define SAMPLE_BIAS 512

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

// This function takes a raw sample (0-511) and maps it to a number from 0-31 for display
// on our RGB matrix.
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

// This function takes our buffer of input samples (time based) and
// displays them on our RGB matrix.
void show_samples_lines( void )
{
  int x;
  int y;
  int last_x;
  int last_y;

  // since we've got more samples (64) than columns in our LED matrix (32), we need
  // to decide which samples to display.  I'm going with a simple version:
  //   only the first 32. 

  // For the first column, start with the y value of that sample.
  last_x = 0;
  last_y = map_sample(sample[0]);

  // now draw the rest.
  for (x=1; x < SAMPLE_SIZE/2; x++)
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

  
}

void collect_samples( void )
{
  int i;
  
  for (i = 0; i < SAMPLE_SIZE; i++)
  {
    sample[i] = analogRead(AUDIO_PIN);
  }
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

void doFHT( void )
{
  int i;
  int temp_sample;
  
  for (i=0; i < SAMPLE_SIZE; i++)
  {
    // Remove DC bias
    temp_sample = sample[i] - SAMPLE_BIAS;

    // Load the sample into the input array
    fht_input[i] = temp_sample;
    
  }
  
  fht_window();
  fht_reorder();
  fht_run();

  // Commented out, because their lin mag functons corrupt memory!!!
  //fht_mag_lin();  
}

// Since it looks like fht_mag_lin is corrupting memory.  Instead of debugging AVR assembly, 
// I'm gonna code my own C version.  
// I'll be using floating point math rather than assembly, so it'll be much slower...
// ...but hopefully still faster than the FFT algos.
int glenn_mag_calc(int bin)
{
  float sum_real_imag=0;
  float diff_real_imag=0;
  float result;
  int   intMag;

  // The FHT algos use the input array as it's output scratchpad.
  // Bins 0 through N/2 are the sums of the real and imaginary parts.
  // Bins N to N/2 are the differences, but note that it's reflected from the beginning.
  sum_real_imag = fht_input[bin];
  if (bin) diff_real_imag = fht_input[FHT_N - bin];
  
  result = (sum_real_imag * sum_real_imag) + (diff_real_imag * diff_real_imag);

  result = sqrt(result);
  result = result + 0.5;  // rounding

  intMag = result;

  return intMag;
  
}

#define MAX_FREQ_MAG 20
void display_freq_raw( void )
{
  int i;
  int mag;
  int x;


  // This works because our sample buffer is 2x the display rows...
  for (i = 0; i < SAMPLE_SIZE/2; i++)
  {
    mag = glenn_mag_calc(i);
    mag = constrain(mag, 0, MAX_FREQ_MAG);
    mag = map(mag, 0, MAX_FREQ_MAG, 0, -8);

    // only have 16 colors, but 32 bins...hence the i/2
    matrix.drawRect(i,32,1,mag, spectrum_colors[i/2]);
  }
 
}

void print_fht_buffer( void )
{
  int i;

  for (i=0; i<FHT_N; i++)
  {
    Serial.println(fht_input[i]);
  }
  Serial.println("====");
  
}

void loop() 
{
  collect_samples();

  matrix.fillScreen(0);
  display_amp_bar();
  show_samples_lines();

  // for some reason, this appears to corrupt data...so I'm doing it as late as possible in the loop.
  doFHT();
  
  display_freq_raw();


  matrix.swapBuffers(true);

}
