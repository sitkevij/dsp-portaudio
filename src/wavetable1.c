/**
 *  Purpose:
 *        update wavetable1.c to be compatible with newer portaudio api
 *
 *  compile:
 *       gcc wavetable1.c -lportaudio -o wavetable1
 *
 *   clang-format:
 *       /Users/julian/bin/clang-format -style=Google -i wavetable1.c
 *
 *  main.c
 *  Wavetable1
 *
 *  Fill a table with values describing a waveform
 *  Then synthesize a tone by reading cyclically through that table
 *  Use simple truncation of calculated sample index
 *  to find the sample value (not as accurate as interpolation)
 *
 *  @see http://music.arts.uci.edu/dobrian/CAMP07/wavetable1.c
 */

#include <stdio.h>
#include <math.h>
#include "portaudio.h"

#define SAMPLE_RATE (44100.)
#define TABLE_LENGTH 1024  // 1024
#define BUFFER_SIZE 256
#define TWOPI (6.283185307179586)
#define NUM_SECONDS (1.)
#define FREQUENCY (440.)
#define MAX_AMP (0.5)

typedef struct {
  float frequency;
  float amplitude;
  float phase;
  float *wavetable;  // pointer to wavetable
  float n;           // current location in table
} wave;              // data to pass to callback function

const float oneoversr = 1. / SAMPLE_RATE;

// fill a table with one cycle of a sine waveform
void filltable(float *table, unsigned long length);
// This routine will be called by the PortAudio engine when audio is needed.
static int sineCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags, void *userData);
int main(int argc, char *argv[]);

void filltable(float *table, unsigned long length) {
  unsigned long i;
  const float twopioverlength = TWOPI / length;  // just calculate once

  for (i = 0; i < length; i++) *(table++) = sin(i * twopioverlength);

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
  float y;         // temp variable for output sample

  for (i = 0; i < framesPerBuffer; i++) {
    y = data->amplitude * (data->wavetable[(int)data->n]);
    // increment the wave's counter
    data->n += data->frequency * TABLE_LENGTH * oneoversr;
    while (data->n > TABLE_LENGTH) data->n -= TABLE_LENGTH;  // keep it in range
    *out++ = y;                                              // left channel
    *out++ = y;                                              // right channel
  }

  return 0;
}

int main(int argc, char *argv[]) {
  PaStreamParameters outputParameters;
  PaStream *stream;
  PaError err;
  wave wave1;                  // my data structure
  float table1[TABLE_LENGTH];  // wavetable

  printf("args %d\n", argc);
  float target_freq;
  sscanf(argv[1], "%f", &target_freq);

  filltable(table1, TABLE_LENGTH);

  printf("PortAudio: wave frequency, %.2f Hz.\n", target_freq);

  // Initialize data for use by callback.
  wave1.frequency = target_freq;
  wave1.amplitude = MAX_AMP;
  wave1.phase = 0.;
  wave1.wavetable = table1;
  wave1.n = 0. + wave1.phase * TABLE_LENGTH;

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
      sineCallback, &wave1);

  if (err != paNoError) goto error;

  err = Pa_StartStream(stream);
  if (err != paNoError) goto error;

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