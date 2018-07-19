//some definitions for pulse modem
#define SAMPLES_IN_FRAME 720  //8Khz PCM samples in 90 mS frame
#define SAMPLESS_IN_SYMBOL 24  //samples in 3-bits symbol
#define SUBFRAMES_IN_FRAME 18  //subframes in frame
#define BITS_IN_FRAME 90      //raw bits in frame
//some definitions for bpsk modem
#define SAMPLESPERWAVE 3  //general for all bitrates
#define WAVESPERBIT 3   //number of waves (carrier half-period) coded one bit.
#define BITSPERSYMBOL 10   //number of raw bits per symbol
#define SYMBOLSPERFRAME 8  //number of symbols per frame

//----------Unions shared memory for pulse and bpsk modems-------------

//frame overlapped buffer
typedef union
{
 short p[2*SAMPLESS_IN_SYMBOL]; //overlapped array between frames
 short b[SAMPLESPERWAVE*WAVESPERBIT]; //overlapped samples buffer
}
t_tail;

//parity bits
typedef union
{
 unsigned char p[SAMPLESS_IN_SYMBOL][SUBFRAMES_IN_FRAME]; //array for received bits on every of 24 lags
 unsigned short b[3*WAVESPERBIT][BITSPERSYMBOL]; //shifted parity bits for frame synchronizing $$$$$
}
t_parity;

//lag weights
typedef union
{
 unsigned int p[SAMPLES_IN_FRAME]; //parity errors in last 10 frames for each lag
 unsigned int b[3*WAVESPERBIT][BITSPERSYMBOL*SYMBOLSPERFRAME];      //weights of lags  $$$$$
}
t_wfr;

//soft bits
typedef union
{
 short p[BITS_IN_FRAME]; //demodulated soft bits in frame
 short b[BITSPERSYMBOL*SYMBOLSPERFRAME];      //metrics of bits
}
t_fd;

//----------State (data buffers) common for pulse and bpsk modems-------------
typedef struct
{
 t_tail tail; //frame overlapped buffer
 t_parity parity; //parity bits
 t_wfr wfr; //lag weights
 t_fd fd; //soft bits
 unsigned char avad;	
}
t_md;
