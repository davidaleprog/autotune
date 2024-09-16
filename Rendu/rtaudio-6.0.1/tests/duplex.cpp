/******************************************/
/*
  duplex.cpp
  by Gary P. Scavone, 2006-2019.

  This program opens a duplex stream and passes
  input directly through to the output.
*/
/******************************************/

#include "RtAudio.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <math.h>

#define PI 3.14159265358979323846

//include somfunc.cpp
#include "../../c/somefunc.cpp"

/*
typedef char MY_TYPE;
#define FORMAT RTAUDIO_SINT8

typedef signed short MY_TYPE;
#define FORMAT RTAUDIO_SINT16

typedef S24 MY_TYPE;
#define FORMAT RTAUDIO_SINT24

typedef signed long MY_TYPE;
#define FORMAT RTAUDIO_SINT32

typedef float MY_TYPE;
#define FORMAT RTAUDIO_FLOAT32
*/
typedef double MY_TYPE;
#define FORMAT RTAUDIO_FLOAT64

// strusture callbackdata that contains fs and bufferBytes
struct duplexData
{
  /* data */
  unsigned int fs;
  unsigned int bufferBytes;
  //buffer_dump
  MY_TYPE *buffer_dump_in;
  MY_TYPE *buffer_dump_out;
  MY_TYPE * circ_buffer;
  int * write_ind;
  int * read_ind;
  unsigned int N_circ_buffer;
  unsigned int n_buff_dump;
  unsigned int n_buff;
  int ind_dump_in;
  int ind_dump_out;
  MY_TYPE * fft_icirc;
  MY_TYPE * fft_rcirc;
  double * harmonics;
  double *phi;
  double f_0_prev;
};

void usage( void ) {
  // Error function in case of incorrect command-line
  // argument specifications
  std::cout << "\nuseage: duplex N fs <iDevice> <oDevice> <iChannelOffset> <oChannelOffset>\n";
  std::cout << "    where N = number of channels,\n";
  std::cout << "    fs = the sample rate,\n";
  std::cout << "    iDevice = optional input device index to use (default = 0),\n";
  std::cout << "    oDevice = optional output device index to use (default = 0),\n";
  std::cout << "    iChannelOffset = an optional input channel offset (default = 0),\n";
  std::cout << "    and oChannelOffset = optional output channel offset (default = 0).\n\n";
  exit( 0 );
}

unsigned int getDeviceIndex( std::vector<std::string> deviceNames, bool isInput = false )
{
  unsigned int i;
  std::string keyHit;
  std::cout << '\n';
  for ( i=0; i<deviceNames.size(); i++ )
    std::cout << "  Device #" << i << ": " << deviceNames[i] << '\n';
  do {
    if ( isInput )
      std::cout << "\nChoose an input device #: ";
    else
      std::cout << "\nChoose an output device #: ";
    std::cin >> i;
  } while ( i >= deviceNames.size() );
  std::getline( std::cin, keyHit );  // used to clear out stdin
  return i;
}

double streamTimePrintIncrement = 1.0; // seconds
double streamTimePrintTime = 1.0; // seconds

/* --------------------------INOUT -----------------------*/

int inout( void *outputBuffer, void *inputBuffer, unsigned int /*nBufferFrames*/,
           double streamTime, RtAudioStreamStatus status, void *data_p )
{ // Since the number of input and output channels is equal, we can do
  // a simple buffer copy operation here.
  if ( status ) std::cout << "Stream over/underflow detected." << std::endl;

  if ( streamTime >= streamTimePrintTime ) {
    std::cout << "streamTime = " << streamTime << std::endl;
    streamTimePrintTime += streamTimePrintIncrement;
  }

  

  duplexData *callback_data = ( duplexData *) data_p; 
  //memcpy( outputBuffer, inputBuffer, bufferBytes );

  int n = callback_data->n_buff;
  unsigned int fs = callback_data->fs;
  double *circ_buffer = callback_data->circ_buffer;
  int N = callback_data->N_circ_buffer;
  int *write_ind = callback_data->write_ind;
  int *read_ind = callback_data->read_ind;
  double *fft_icirc = callback_data->fft_icirc;
  double *fft_rcirc = callback_data->fft_rcirc;
  
  //fill circular buffer
  for (int i = 0; i < n; i++) {
    circ_buffer[*write_ind] = ((double *) inputBuffer)[i];
    *write_ind = (*write_ind + 1) % N;
  }

  // update write index
  callback_data->write_ind = write_ind;

  double *phi = callback_data->phi;

  // extract f_0
  double f_0 = extract_f0( circ_autocor( circ_buffer, N,read_ind),
                          N,fs, 50, 500);


  // fft
  for (int i = 0; i < N; i++) {
    fft_rcirc[i] = circ_buffer[(*read_ind + i) % N];
  }
  fftr(fft_rcirc, fft_icirc, N);
  // check with ifft : ifftr(fft_rcirc, fft_icirc, N); 



  // extract harmonics
  double * harmonics = callback_data->harmonics;
  int n_harm = 10;
  for ( int j = 1; j < n_harm+1; j++){
    for ( int i = 0; i < N; i ++){
      if (i == int(j*f_0*N/fs)){
      harmonics[j-1] = sqrt(fft_rcirc[i]*fft_rcirc[i] + fft_icirc[i]*fft_icirc[i]);
      }
    }
  }
 
  // round f_0 to autotune
  int nb_levels = 12;
  double f_st = nb_levels*std::log2(f_0);
  f_0 = std::pow(2, std::round(f_st)/nb_levels);


  //synthesis
  double f_0_prev = callback_data->f_0_prev;
  for (int i = 0; i < n; i++) { 
    ((double *) outputBuffer)[i] = 0;
    for (int j = 0; j < n_harm; j++){
      ((double *) outputBuffer)[i] += harmonics[j]*sin(2*PI*(j+1)*f_0*(i+1)/fs +phi[j])*1/N;
    }
  }

  //update phi
  for (int j = 0; j < n_harm; j++){
    phi[j] += 2*PI*(j+1)*f_0_prev*double(n)/fs;}
  callback_data->phi = phi;

  //update f_0_prev
  callback_data->f_0_prev = f_0;

  //memcpy( outputBuffer, inputBuffer, bufferBytes );
  write_buff_dump((double *) outputBuffer, n, callback_data->buffer_dump_out,
  callback_data->n_buff_dump, &(callback_data->ind_dump_out));
  
  write_buff_dump((double *) inputBuffer, n, callback_data->buffer_dump_in, 
  callback_data->n_buff_dump, &(callback_data->ind_dump_in));

  // update read index
  *callback_data->read_ind = (*read_ind + n) % N;
  return 0;
}



int main( int argc, char *argv[] )
{
  unsigned int channels, fs, bufferBytes, oDevice = 0, iDevice = 0, iOffset = 0, oOffset = 0;

  // Minimal command-line checking
  if (argc < 3 || argc > 7 ) usage();

  RtAudio adac;
  std::vector<unsigned int> deviceIds = adac.getDeviceIds();
  if ( deviceIds.size() < 1 ) {
    std::cout << "\nNo audio devices found!\n";
    exit( 1 );
  }

  channels = (unsigned int) atoi(argv[1]);
  fs = (unsigned int) atoi(argv[2]);
  if ( argc > 3 )
    iDevice = (unsigned int) atoi(argv[3]);
  if ( argc > 4 )
    oDevice = (unsigned int) atoi(argv[4]);
  if ( argc > 5 )
    iOffset = (unsigned int) atoi(argv[5]);
  if ( argc > 6 )
    oOffset = (unsigned int) atoi(argv[6]);

  // Let RtAudio print messages to stderr.
  adac.showWarnings( true );

  // Set the same number of channels for both input and output.
  unsigned int bufferFrames = 512;
  RtAudio::StreamParameters iParams, oParams;
  iParams.nChannels = channels;
  iParams.firstChannel = iOffset;
  oParams.nChannels = channels;
  oParams.firstChannel = oOffset;

  if ( iDevice == 0 )
    iParams.deviceId = adac.getDefaultInputDevice();
  else {
    if ( iDevice >= deviceIds.size() )
      iDevice = getDeviceIndex( adac.getDeviceNames(), true );
    iParams.deviceId = deviceIds[iDevice];
  }
  if ( oDevice == 0 )
    oParams.deviceId = adac.getDefaultOutputDevice();
  else {
    if ( oDevice >= deviceIds.size() )
      oDevice = getDeviceIndex( adac.getDeviceNames() );
    oParams.deviceId = deviceIds[oDevice];
  }
  
  RtAudio::StreamOptions options;
  //options.flags |= RTAUDIO_NONINTERLEAVED;

  bufferBytes = bufferFrames * channels * sizeof( MY_TYPE );

  //create callback_data
  duplexData data;
  data.fs = fs;
  data.bufferBytes = bufferBytes;
  data.n_buff = bufferFrames*channels;
 

  //create buffer_dump
  unsigned int time_recorded = 10;
  unsigned int nb_buffers = int(time_recorded*fs/bufferFrames); //number of buffers to record in buffer_dump
  unsigned int n_buff_dump = nb_buffers*bufferFrames*channels; //total number of frames in buffer_dump
  data.buffer_dump_in = (MY_TYPE *) calloc(n_buff_dump, sizeof(double));
  data.buffer_dump_out = (MY_TYPE *) calloc(n_buff_dump, sizeof(double));

  //create circular buffer
  data.N_circ_buffer = bufferFrames*4; //4 buffers in circular buffer
  double* circ_buffer = (double *) calloc(data.N_circ_buffer, sizeof(double));
  data.circ_buffer = circ_buffer;
  
  data.fft_icirc = (double *) calloc(data.N_circ_buffer, sizeof(double));
  data.fft_rcirc = (double *) calloc(data.N_circ_buffer, sizeof(double));

  data.write_ind = (int *) calloc(1, sizeof(int));
  data.read_ind = (int *) calloc(1, sizeof(int));
  *data.write_ind = 0;
  *data.read_ind = 0;
  data.phi = (double *) calloc(10, sizeof(double)); //10 harmonics
  data.f_0_prev = 0;

  double * harmonics = (double *) calloc(10, sizeof(double)); //10 harmonics
  data.harmonics = harmonics;

  if (data.buffer_dump_in == NULL) {
    printf("Error: could not allocate memory for buffer_dump\n");
    exit(1);
  }
  if (data.buffer_dump_out == NULL) {
    printf("Error: could not allocate memory for buffer_dump\n");
    exit(1);
  }

  data.n_buff_dump = n_buff_dump;

  data.ind_dump_in = 0;
  data.ind_dump_out = 0;
  if ( adac.openStream( &oParams, &iParams, FORMAT, fs, &bufferFrames, &inout, (void *)&data, &options ) ) {
    goto cleanup;
  }

  if ( adac.isStreamOpen() == false ) goto cleanup;

  // Test RtAudio functionality for reporting latency.
  std::cout << "\nStream latency = " << adac.getStreamLatency() << " frames" << std::endl;

  if ( adac.startStream() ) goto cleanup;

  char input;
  std::cout << "\nRunning ... press <enter> to quit (buffer frames = " << bufferFrames << ").\n";
  std::cin.get(input);

  // Stop the stream.
  if ( adac.isStreamRunning() ) adac.stopStream();
  
  

 cleanup:
  if ( adac.isStreamOpen() ) adac.closeStream();

  FILE* file = fopen("buffer_dump_in.bin", "wb");
  if (file != NULL) {
      fwrite(data.buffer_dump_in, sizeof(MY_TYPE), data.n_buff_dump, file);
      fclose(file);
  }
  else {
      printf("Error: could not open file for writing\n");
      exit(1);
  }
  free(data.buffer_dump_in);

  FILE* file2 = fopen("buffer_dump_out.bin", "wb");
  if (file2 != NULL) {
      fwrite(data.buffer_dump_out, sizeof(MY_TYPE), data.n_buff_dump, file2);
      fclose(file2);
  }
  else {
      printf("Error: could not open file for writing\n");
      exit(1);
  }
  return 0;
}
