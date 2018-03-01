/**
 *  Purpose:
 *    modernize wavetable2.c to be compatible with newer portaudio api
 *
 *  gcc compile:
 *    gcc wavetable2.c -lportaudio -o wavetable2
 *
 *  clang-format:
 *    /Users/julian/bin/clang-format -style=Google -i wavetable1.c
 *
 *  main.c
 *  Wavetable2
 *
 *  Fill a table with values describing a waveform. Then synthesize a tone by
 *  reading cyclically through that table. Calculate the actual sample value by
 *  means of linear interpolation between two adjacent sample indices (more
 *  accurate than simple truncation of sample index).
 *
 *  @see original code http://music.arts.uci.edu/dobrian/CAMP07/wavetable2.c
 *
 */

#include <stdio.h>
#include <math.h>
#include "portaudio.h"

#define SAMPLE_RATE (44100.)
#define TABLE_LENGTH 1024
#define BUFFER_SIZE 256
#define TWOPI (6.283185307179586)
#define NUM_SECONDS (4.)
#define FREQUENCY (440.)
#define MAX_AMP (0.5)

typedef struct {
  float frequency;
  float amplitude;
  float phase;
  float *wavetable;
  float n;
} wave;  // data to pass to callback function

const float oneoversr = 1. / SAMPLE_RATE;

// fill a table with one cycle of a sine waveform
void filltable(float *table, unsigned long length);
// This routine will be called by the PortAudio engine when audio is needed.
static int sineCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags, void *userData);
int main(void);

void filltable(float *table, unsigned long length) {
  unsigned long i;
  const float twopioverlength = 8. * atan(1.) / length;  // or TWOPI/length

  for (i = 0; i < length; i++) *(table++) = sin(i * twopioverlength);
  *table = 0.;  // final (returning) point for interpolation purposes

  return;
}

static int sineCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags, void *userData) {
  /* Cast data passed through stream to the format of the local structure. */
  wave *data = (wave *)userData;
  // float *in = (float*)inputBuffer; // input buffer only needed for input
  float *out = (float *)outputBuffer;

  unsigned int i;  // a counter
  float f;         // hold fractional part of sample index
  float y;         // temp variable for output sample

  for (i = 0; i < framesPerBuffer; i++) {
    f = data->n - (int)data->n;  // get fractional part of index
    // use it to interpolate between the two closest sample indices
    y = data->amplitude * ((1. - f) * (data->wavetable[(int)data->n]) +
                           f * (data->wavetable[(int)data->n + 1]));
    data->n += data->frequency * TABLE_LENGTH *
               oneoversr;  // increment the wave's counter
    while (data->n > TABLE_LENGTH) data->n -= TABLE_LENGTH;  // keep it in range
    *out++ = y;                                              /* left channel */
    *out++ = y;                                              /* right channel*/
  }

  return 0;
}

int main(void) {
  PaStreamParameters outputParameters;
  PaStream *stream;
  PaError err;
  wave wave2;                      // my data structure
  float table2[TABLE_LENGTH + 1];  // wavetable with final interpolation point

  filltable(table2, TABLE_LENGTH);

  printf("PortAudio: Sine Wave, %.2f Hz.\n", FREQUENCY);

  // Initialize data for use by callback.
  wave2.frequency = FREQUENCY;
  wave2.amplitude = MAX_AMP;
  wave2.phase = 0.;
  wave2.wavetable = table2;
  wave2.n = 0. + wave2.phase * TABLE_LENGTH;

  // Initialize library before making any other calls.
  err = Pa_Initialize();
  if (err != paNoError) goto error;

  outputParameters.device =
      Pa_GetDefaultOutputDevice(); /* default output device */
  if (outputParameters.device == paNoDevice) {
    fprintf(stderr, "Error: No default output device.\n");
    goto error;
  }
  outputParameters.channelCount = 2;         /* stereo output */
  outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
  outputParameters.suggestedLatency =
      Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
  outputParameters.hostApiSpecificStreamInfo = NULL;

  // Open an audio I/O stream.
  err = Pa_OpenStream(
      &stream, NULL,                               /* no input */
      &outputParameters, SAMPLE_RATE, BUFFER_SIZE, /* frames per buffer */
      paClipOff, /* number of buffers, if zero then use default minimum */
      sineCallback, &wave2);

  if (err != paNoError) goto error;

  err = Pa_StartStream(stream);
  if (err != paNoError) goto error;

  // Sleep for several seconds.
  Pa_Sleep(NUM_SECONDS * 1000.);

  err = Pa_StopStream(stream);
  if (err != paNoError) goto error;

  err = Pa_CloseStream(stream);
  if (err != paNoError) goto error;

  Pa_Terminate();
  printf("Finished.\n");

  return err;

error:
  Pa_Terminate();
  fprintf(stderr, "An error occured while using the portaudio stream.\n");
  fprintf(stderr, "Error number: %d\n", err);
  fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
  return err;
}