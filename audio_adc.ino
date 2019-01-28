// Sampling for mega.
// First cut...just prints. 

//  We're using A5 as our audio input pin.
//  To use the (slower) analogRead(), comment out this define.  Otherwise we do a faster bitbang of the ADC.
//  NOTE:  If you are doing bit-banging, you need to connect 5v ref to AREF pin on the mega.
#define BIT_BANG_ADC

#define MUX_MASK 0x05
#define AUDIO_PIN A5

// These are the raw samples from the audio input.
#define SAMPLE_SIZE 32
int sample[SAMPLE_SIZE] = {0};

void setup() 
{
  Serial.begin(9600);
  
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
  unsigned long start_time;
  unsigned long stop_time;
  unsigned long delta_t;
  unsigned long per_sample;

  start_time = micros();
  
  #ifdef BIT_BANG_ADC
  collect_accurate_samples();
  #else
  collect_samples();
  #endif
  
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
