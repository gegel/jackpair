///////////////////////////////////////////////
//
// **************************
//
// Project/Software name: X-Phone
// Author: "Van Gegel" <gegelcopy@ukr.net>
//
// THIS IS A FREE SOFTWARE  AND FOR TEST ONLY!!!
// Please do not use it in the case of life and death
// This software is released under GNU LGPL:
//
// * LGPL 3.0 <http://www.gnu.org/licenses/lgpl.html>
//
// You’re free to copy, distribute and make commercial use
// of this software under the following conditions:
//
// * You have to cite the author (and copyright owner): Van Gegel
// * You have to provide a link to the author’s Homepage: <http://torfone.org>
//
///////////////////////////////////////////////


//800 bps pulse modem V3.1 fixed point  June 2018


//This file contains procedures for modulate and demodulate data using pseudo-voice pulse modem
//suitable for AMR and GSM_FR compressed channels
//This is special pulse modem with carrier parameters strongly adapted for codec engine with VAD
//Some tricks are empirically discovered and can be not optimal. Now modem in development and can be
//improved with new ideas. Also code can be not optimal by performance and style due to continuous changes,
//Fixing point is used, optimized for 32-bits platform


//Modulator:
//input frame is 9 bytes (72 data bits) splitted to 18 subframes 4 data bits in each.
//For each subframe one parity bits computes so one subframe contains 5 bits total
//and 18*5=90 raw bits will be modulated. During modulation first output 18 bits 0 of all 18 subframes, then bits 1...
//and last 18 parity bits at the end of frame.
//90 modulated bits splitted to 30 symbols 3 bits in each.
//each symbol contains 24 8KHz PCM samples, so whole frame contains 720 samples (90 mS).
//Each symbol contains one pulse wave can be in one of four possible positions:
//occupie samples 0,1,2  or 6,7,8, or 12,13,14, or 18,19,20.  This position codes by 2 bits of symbol.
//third bits codes the pulse polarity: 0 for positive pulse or 1 for negative.


//Demodulator:

//The first, Demodulator compute energy of frame as a difference of 0-3, 1-4, 2-5 ... samples
//over whole frame (for normalisation of correlation coefficients)

//After this all 720 samples will be processes sample by sample. While next sample processes we
//consider that this is the first sample of symbol and provide fast detecting of symbol's
//hard bits. 4 correlation coefficients will be computed for each possible
//pulse position. For fast we search absolute greats coefficient and get 2 bits
//depends position and third bit depend sign (polarity).
//Note: only one of 24 subsequent symbols (in real start position) will be correct, other will be wrong.
//We store symbols separately for all 24 positions in 24 arrays.
//After symbol was processed we suppose this was a last symbol in the frame. So we check parity of all subframes in array
//and compute number of parity errors. We add this number to one of 720 FIFOs correspond current position of sample in the frame.
//Only one position will be best -  the start of last symbol in the frame.
//In this position the number of error for last 10 frames in FIFO will be minimal.
//After all 720 samples of the next frame will be processed we search the best position (best lag) in the range 0-719 samples
//and consider this is the start of last symbol of the frame.
//After new frame will be processed, at %24 of this position we provide fine detecting of symbols and store soft bits in array.
//Exactly in best lag position (the end of frame) we  extract all soft bits from array
//normalize for output, provide hard decision and soft FEC.

//So our modem is stream modem with fast self synchronizing (less then 10 frames needs for correct output)
//with minimal overhead 4/5 used both for synchronizing and soft FEC.

//Pulse carrier contains 1 pulse to each 24 samples. This is suitable for
//algebraic code book of AMR codec in 4.75kbps mode: for one AMR codec subframe of 40 samples codebook can code 2
//pulses each in 8 possible positions with sign.
//So our carrier is robust for AMR compression and can be used via cellular phones.


#include <stdlib.h> //abs
#include <string.h> //memcpy, memset

#include "modem.h"  //interface
#include "mdmdata.h" //structure definitions

//modem definitions
//=====================================================================================
//Frame format for modulator's symbols
#define SAMPLES_IN_FRAME 720  //8Khz PCM samples in 90 mS frame
#define SAMPLESS_IN_SYMBOL 24  //samples in 3-bits symbol
#define SYMBOLS_IN_FRAME 30    //symbols in the frame
#define POS_IN_SYMBOL 4        //possible pulse positions in symbol
#define BITS_IN_SYMBOL 3       //bits coded by symbol (by pulse position and polarity)
//Frame format for FEC
#define BITS_IN_FRAME 90      //raw bits in frame
#define SUBFRAMES_IN_FRAME 18  //subframes in frame
#define BITS_IN_SUBFRAME 5     //raw bits in subframe
#define DATA_IN_SUBFRAME 4     //data bits in subframe  (and one bit is parity)
//demodulator levels
#define MAXTUNE 64  //coefficient for sample rate tuning
#define BERBITS  50 //number of error for twice decresing values
#define LOCK_TRS 100 //quality level for set lock flag (1-64)
#define UNLOCK_TRS 100 //quality level for clear lock flag(0-63)
#define LAGSEQ 0       //number of sequentialy mautched lags for apply to modem (0-4)
//LLR computation
#define LLR_MAX           ((int)(0x7fff-1)) //maximal LLR
#define LOGEXP_RES        (401)           // resolution of logexp table
//Helpers
#define MAX(a,b) ((a)>(b) ? (a) : (b))    // macro: maximum
#define MIN(a,b) ((a)<(b) ? (a) : (b))    // macro: minimum
#define ABS(a)   ((a)< 0 ? -(a) : (a))    // macro: absolute value
#define SIGN(a)  ((a)< 0 ? (-1) : (1))    // macro: sign


//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//undefine option for detect pulse by rising edge
//now I don't undestand what is better in such cases
//that's why this option require additional research yet 
#define MIRROR
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

 //demodulator data arrays
 //short ptail[2*SAMPLESS_IN_SYMBOL]; //overlapped array between frames
 //unsigned char parity[SAMPLESS_IN_SYMBOL][SUBFRAMES_IN_FRAME]; //array for received bits on every of 24 lags
 //unsigned int pwfr[SAMPLES_IN_FRAME]; //parity errors in last 10 frames for each lag
 //short pfd[BITS_IN_FRAME]; //demodulated soft bits in frame
 extern t_md md; //demodulator's state (shared with bpsk modem)

 //lag searching values
 static short pbestlag=0; //(0-719) //best lag points to 696-th sample in the frame (start of last symbol)
 static unsigned int plagseq=0; //sequence (FIFO) for four last quality values
 static unsigned char plagflg=0; //flag of carrier locked

 //counters for estimate parity errors rate will be near 5% for noise
 static unsigned int pbit_cnt=1; //counter of receiver raw bits (must be non-zero for division)
 static unsigned int pbit_err=0; //counter of parity bits errors (max is 8, noise is 4 errors to 80 raw bits)


 //counter of modulated frames for anti-vad trick
 static unsigned char mdcnt=0;

 //Channel polarity autodetect
 unsigned char plt=1;  //channel polarity (0/1)
 unsigned short pltcnt=0;  //counter of symbols with wrong polarity
 
 //Sample rate fine tuning
 int ar=0; //amplitudes of left samples of peaks
 int al=0; //amplitudes of right samples of peak

//----------------------------TABLES-----------------------------

//order of bits in symbol for modulator
static const unsigned char revtab[8]={0,4,2,6,1,5,3,7};

//order of bits in symbol for fast hard demodulator
static const unsigned char fasttab[8]={3,0,1,2,4,7,6,5};

//number of set bits in nibble (for check parity)
static const unsigned char SetBitTable[16]={0,1,1,2, 1,2,2,3, 1,2,2,3, 2,3,3,4};

//bit's mask in byte
static const unsigned char mask[8]={1,2,4,8,16,32,64,128}; //bit mask table

//lag quality depends number of bits errors (for demodulator)
static const unsigned char ltab[8]={64,32,16,8,4,2,1,0};

//modulation symbol waveform
static const short ulPulse[24] =
{
  0,     0,     0,     0,     40,    -200,
  560, -991,  -1400, 7636,  15000, 7636,
  -1400, -991,  560, -200,  40,    0, 
  0,     0,     0,     0,     0,     0
};

//index of bits in symbol for fine soft demodulator
static const short indexBits[24] =
{
  0, 0, 0,
  0, 0, 1,
  0, 1, 0,
  0, 1, 1,
  1, 0, 0,
  1, 0, 1,
  1, 1, 0,
  1, 1, 1
};

//Jacobian table (Q8)
static const short logExpTable[LOGEXP_RES] =
{
  177, 175, 173, 172, 170, 168, 166, 164, 162, 160,
  158, 156, 155, 153, 151, 149, 147, 146, 144, 142,
  141, 139, 137, 136, 134, 132, 131, 129, 128, 126,
  124, 123, 121, 120, 118, 117, 115, 114, 113, 111,
  110, 108, 107, 106, 104, 103, 102, 100, 99,  98,
  96,  95,  94,  93,  92,  90,  89,  88,  87,  86,
  85,  83,  82,  81,  80,  79,  78,  77,  76,  75,
  74,  73,  72,  71,  70,  69,  68,  67,  66,  65,
  64,  64,  63,  62,  61,  60,  59,  59,  58,  57,
  56,  55,  55,  54,  53,  52,  52,  51,  50,  49,
  49,  48,  47,  47,  46,  45,  45,  44,  43,  43,
  42,  42,  41,  40,  40,  39,  39,  38,  38,  37,
  37,  36,  35,  35,  34,  34,  33,  33,  32,  32,
  32,  31,  31,  30,  30,  29,  29,  28,  28,  28,
  27,  27,  26,  26,  26,  25,  25,  25,  24,  24,
  23,  23,  23,  22,  22,  22,  21,  21,  21,  21,
  20,  20,  20,  19,  19,  19,  18,  18,  18,  18,
  17,  17,  17,  17,  16,  16,  16,  16,  15,  15,
  15,  15,  14,  14,  14,  14,  14,  13,  13,  13,
  13,  13,  12,  12,  12,  12,  12,  12,  11,  11,
  11,  11,  11,  11,  10,  10,  10,  10,  10,  10,
  9,   9,   9,   9,   9,   9,   9,   8,   8,   8,
  8,   8,   8,   8,   8,   7,   7,   7,   7,   7,
  7,   7,   7,   7,   7,   6,   6,   6,   6,   6,
  6,   6,   6,   6,   6,   6,   5,   5,   5,   5,
  5,   5,   5,   5,   5,   5,   5,   5,   5,   4,
  4,   4,   4,   4,   4,   4,   4,   4,   4,   4,
  4,   4,   4,   4,   4,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   2,   2,   2,   2,
  2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
  2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
  2,   2,   2,   2,   2,   2,   2,   2,   2,   1,
  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
  0
};

//Helpers
//==================================================================================


//MAX function for sum of LLRs in log domain
short JacLog(int a, int b)
{
  int ind; //index in Jacobian table depend difference of values
  int maxab; //correction value from table

  //choose maximal value and compute absolute difference between values
  if (a > b)
  {
    maxab = a;         //set maximal value to output
    ind = (a-b) >> 2; //normalize for table format
  }
  else
  {
    maxab = b;          //set maximal value to output
    ind = (b-a) >> 2;  //normalize for table format
  }

  //correct output with value extracted from table by index
  maxab += (ind >= LOGEXP_RES) ? 0 : logExpTable[ind];

  if (maxab >= LLR_MAX) return LLR_MAX;  //saturate
  else return (short)maxab; //return resulting LLR
}

//Symbol processing
//=================================================================================
//modulate symbol (0-7) to 24 short samples
void SymbolMod(unsigned char symbol, short *mPulse)
{
  short i, shifter;

  // initialize memory 
  for (i = 0; i < 24; i++) mPulse[i] = 0;
  //check MSB for set polarity
  if (symbol < 4)  // creates positive symbols
  {
    shifter = 6*symbol; //set pointer to wave from the start of table
    for (i = 0; i < 24; i++) mPulse[(i+shifter)%24] = ulPulse[i]; //copy 24 samples from table with specified shift
  }
  else  //creates negative symbols
  {
    shifter = 6*(7-symbol); //set pointer to wave from the end of table
    for (i = 0; i < 24; i++) mPulse[(i+shifter)%24] = (-1)*(ulPulse[i]); //copy 24 samples from table with specified shift
  }
  
}

//convert 4 correlation coefficients to 3 soft bits
//output demodulated symbol contain 3 hard bits
unsigned char SymbolLLR(unsigned char sym, int* rr, short* softBits)
{
  short i, j, k, m; //counters
  unsigned char c;  //bits
  int Lacc, r; //LLR, correlation coeeficient
  int lp[BITS_IN_SYMBOL]; //array for positive LLR
  unsigned char bp[BITS_IN_SYMBOL] = {0, 0, 0};  //flags data already exist
  int lm[3]; //array for negative LLR
  unsigned char bm[BITS_IN_SYMBOL] = {0, 0, 0}; //flags data already exist

  //for each pulse position
  for (i = 0; i < POS_IN_SYMBOL; i++)
  {
   r=rr[(i+1)&3]; //get correlation coefficient depends modulator's waves
   //consider positive and negative copy
//===========================================================================
   for (j = 0; j <= 1; j++) //0-positive, 1-negative
   {
    if(j) m=BITS_IN_SYMBOL*(7-i); else m=BITS_IN_SYMBOL*i; //m is position in bit table
    if(j) r=-r; //invert correlation coefficient for negative copy

    //compute LACC from r with saturation
    Lacc = r;
    if (ABS(Lacc) > LLR_MAX) Lacc = SIGN(Lacc)*LLR_MAX;

    //cumulative sum of logexp values for 3 bits in symbol
    for (k = 0; k < BITS_IN_SYMBOL; k++) //for each bit
    {
     if (indexBits[m+k]) //check bit flag in index table
     {       //if flag is set use LLL+ array
      if(!bp[k]) //check this cell is empty
      {
       lp[k] = Lacc; //store LLR in cell
       bp[k] = 1;    //clear empty flag
      }
      else lp[k] = (int)JacLog(lp[k], Lacc); //if cell is not empty add new LLR to existed
     }
     else //if bit flag is clear use LLR- array
     {
      if (!bm[k])  //check this cell is empty
      {
       lm[k] = Lacc; //store LLR in cell
       bm[k] = 1; //clear empty flag
      } else lm[k] = (int)JacLog(lm[k], Lacc); //if cell is not empty add new LLR to existed
     } //end of no LLR in cell yet
    } //end of bit loop for 3 symbol's bits
//===========================================================================
   }  //end of loop for positive/negative copy
  } //end of loop of processing 4 correlation coefficients

  if(sym==0xFF) //provide hard decision of LLR for output hard bits
  {
   //determine final LLR values using specified hard decision
   c=0;  //symbol's hard bits
   for (k = 0; k < 3; k++)  //for each bit
   {
    Lacc = lm[k] - lp[k]; //compute resulting LLR as difference between positive and negative copy
    if (ABS(Lacc) > LLR_MAX)  softBits[2-k] = (short)(SIGN(Lacc)*LLR_MAX); //saturate
    else if(!Lacc) softBits[2-k]=1; //must be non-zero for obtain sign
    else softBits[2-k] = (short)Lacc; //output LLR value with sign (soft bit)
    c<<=1; //shift bits
    if(softBits[2-k]<0) c|=1; //add hard decision to symbol
   }
  }
  else //use external hard decision to set sign of soft bits
  {
   //compute hard decision and determine final LLR values
   c=sym;  //external hard bits of symbol
   for (k = 0; k < 3; k++) //process for 3 bits
   {
    Lacc = abs(lm[2-k]-lp[2-k]); //compute resulting LLR as absolute difference between positive and negative copy
    if(!Lacc) Lacc++; //must be non-zero for obtain sign
    if(Lacc > LLR_MAX) Lacc=LLR_MAX; //saturate
    if(sym&1) Lacc=-Lacc; //set sign of soft bit depends external hard bit
    sym>>=1;  //shift external hard bits to next
    softBits[k] = Lacc;  //set soft bit
   }
  }
  return c; //returns 3 hard bits of symbol (first bit is bit 0)
}



//fast demodulate 24 samples (symbol):
//compute 4 correlation coefficients for possible pulse positions and polarity
//returns 3 hard bits of demodulated symbol
unsigned char SymbolDemod(const short* mPulse, int* r)
{
 int d, k; //best and current amplitude of pulse
 short i, b; //counter and best pulse position
 if(plt)
 {
 //compute correlation coefficients for each possible pulse position
 r[0]=(int)mPulse[0] + (((int)mPulse[1])<<1) + (int)mPulse[2] - (int)mPulse[3] - (((int)mPulse[4])<<1) - (int)mPulse[5];
 r[1]=(int)mPulse[0+6] + (((int)mPulse[1+6])<<1) + (int)mPulse[2+6] - (int)mPulse[3+6] - (((int)mPulse[4+6])<<1) - (int)mPulse[5+6];
 r[2]=(int)mPulse[0+12] + (((int)mPulse[1+12])<<1) + (int)mPulse[2+12] - (int)mPulse[3+12] - (((int)mPulse[4+12])<<1) - (int)mPulse[5+12];
 r[3]=(int)mPulse[0+18] + (((int)mPulse[1+18])<<1) + (int)mPulse[2+18] - (int)mPulse[3+18] - (((int)mPulse[4+18])<<1) - (int)mPulse[5+18];
 }
 else
 {
  //compute correlation coefficients for each possible pulse position
  r[0]=(int)mPulse[3] + (((int)mPulse[4])<<1) + (int)mPulse[5] - (int)mPulse[0] - (((int)mPulse[1])<<1) - (int)mPulse[2];
  r[1]=(int)mPulse[3+6] + (((int)mPulse[4+6])<<1) + (int)mPulse[5+6] - (int)mPulse[0+6] - (((int)mPulse[1+6])<<1) - (int)mPulse[2+6];
  r[2]=(int)mPulse[3+12] + (((int)mPulse[4+12])<<1) + (int)mPulse[5+12] - (int)mPulse[0+12] - (((int)mPulse[1+12])<<1) - (int)mPulse[2+12];
  r[3]=(int)mPulse[3+18] + (((int)mPulse[4+18])<<1) + (int)mPulse[5+18] - (int)mPulse[0+18] - (((int)mPulse[1+18])<<1) - (int)mPulse[2+18];
 }

 //search position with best amplitude
 d=0; b=0;
 for(i=0;i<4;i++) //check for each position
 {
  k=abs(r[i]); //amplitude of pulse in this position
  if(k>d) //if this is a best amplitude
  {
   d=k; //set this amplitude as best
   b=i; //set this position as best
  }
 }

 if(r[b]<0) b+=4;  //check polarity of best pulse and set third bit of symbol
 return fasttab[b]; //convert to actual symbol bits
}


//modulate 72 data bits to 720 pcm samples
//add 18 parity bits at the end of frame
//pulse modulator: each 3 bits codes by one pulse in 4 possible positions
//in 24 pcm samples (pulse can be positive or negative)
void Modulate_p(unsigned char* data, short* frame)
{


    short i, j, k;  //counters
    short bn;  //number of input data bit
    unsigned char txp[SUBFRAMES_IN_FRAME]={0,}; //subframe's parity bits
    unsigned char b; //bits of outputted symbol

    //process all bits will be modulated
    for(k=0;k<BITS_IN_FRAME;k++) //number of output bit in frame
    {

     j=k%SUBFRAMES_IN_FRAME; //source subframe for current output
     i=k/SUBFRAMES_IN_FRAME; //source data bit for current output in subframe
     b<<=1; //shift symbol's bits

     if(i!=DATA_IN_SUBFRAME)  //check this is data bit or parity bit
     {
      bn=j*DATA_IN_SUBFRAME+i; //number of requested input data bit
      if(data[bn/8]&mask[bn%8]) b|=1; //get data bit by number from input
      txp[j]^=(b&1); //add this bit to parity of this subframe
     }
     else b|=txp[(SUBFRAMES_IN_FRAME-1)-j];  //this is parity bit for subframe (transmit reverse)


      //modulate every 3 bits to 24 pcm samples (one symbol)
     if((k%3)==2) SymbolMod(revtab[b&7], frame+8*(k-2));
   }
    //anti-vad trick: mute every even frames
	 if((md.avad)&&(1&mdcnt++)) for(i=0;i<SAMPLES_IN_FRAME;i++) frame[i]>>=1;
	
}


//demodulate 720 pcm samples to 9 bytes and 72 corresponding LLR
//output array must be 16 bytes (for flags and statistic)
//returns signed value+-8 to fine tune sampling rate by hardware 
short Demodulate_p(short* frame, unsigned char* data, unsigned char* fout)
{
 short i, j; //general counters
 int m, r, l; //energy of sides of pulses for centring to pulse by tuning ADC timer (sampling rate)
 short peak=0; //energy lag
 int e; //frame energy of 1333Hz (for normalize correlation coefficients)
 short scnt; //counter of processed samples
 short* sptr; //pointer to processed symbol
 int rr[POS_IN_SYMBOL]; //correlation coefficients for every pulse position
 unsigned char a, b, c; //bits
 unsigned char tsym[SUBFRAMES_IN_FRAME]; //table for inverse parity bit location
 short lag, sbf, d, k, q; //24-pcm lag and number of virtual subframe
 short bnum; //number of current bit
 int m_s;  //average LLR, quality multiplier
 int z; //average LLR in this frame (frame quality)
 unsigned char ber; //errors counter in outputted frame
 unsigned int u; //FIFO of number of errors in last 10 frames

 //Compute frame energy on 1333Hz for normalize correlation coefficients
 e=1; //initial value must be non-zero for always allow to be a divider
 for(i=0;i<SAMPLES_IN_FRAME-3;i++) //process all samples in the frame
 {
  e+=(abs((int)frame[i]-(int)frame[i+3])); //sum wave amplitudes
 }
 e>>=3; //divide for match Q

//---------------------------------------------------------------------
 //Copy first symbol to second half of tail
 memcpy(md.tail.p+SAMPLESS_IN_SYMBOL, frame, SAMPLESS_IN_SYMBOL*sizeof(short));

 //---------------------------------------------------------------------
 //process 720 pcm positions in loop
 for(scnt=0;scnt<SAMPLES_IN_FRAME;scnt++)
 {
  //set pointer to symbol (24 pcm) will be demodulated
  if(scnt<SAMPLESS_IN_SYMBOL) sptr=md.tail.p+scnt; else sptr=frame+scnt-SAMPLESS_IN_SYMBOL; //first 24 pcm is in tail
  //fast demodulate symbol to 3 bits
  a=SymbolDemod(sptr, rr);

//---------------------------------------------------------------------
  //set values for check parity at lag of this sample
  b=a; //demodulated bits of symbol in current lag
  lag=scnt%SAMPLESS_IN_SYMBOL; //lag of current virtual symbol in samples from frame start, 0-23
  sbf = ((scnt/SAMPLESS_IN_SYMBOL)%(SAMPLESS_IN_SYMBOL/POS_IN_SYMBOL))*BITS_IN_SYMBOL; //number of current virtual subframe 0-15

  //add 3 bits to set of virtual subframe for current lag
  md.parity.p[lag][sbf]<<=1; //shift bits of subframe for this lag
  md.parity.p[lag][sbf++]|=(b&1); b>>=1; //add received bit, shift received bits
  md.parity.p[lag][sbf]<<=1;
  md.parity.p[lag][sbf++]|=(b&1); b>>=1;
  md.parity.p[lag][sbf]<<=1;
  md.parity.p[lag][sbf++]|=(b&1); b>>=1;

//---------------------------------------------------------------------
  //set reverse order of parity bits
  for(i=0;i<SUBFRAMES_IN_FRAME;i++) //18 parity bits: one for each subframe
  {
   tsym[(sbf+i)%SUBFRAMES_IN_FRAME] =
    (sbf+SUBFRAMES_IN_FRAME-1-i)%SUBFRAMES_IN_FRAME; //set in reverse order
  }

  //count parity errors for this lag(0-18), add to FIFO
  b=0; //clear counter
  for(i=0;i<SUBFRAMES_IN_FRAME;i++) //check parity of all subframes
  {
   c=SetBitTable[0x0F & (md.parity.p[lag][i]>>1)]; //checksum of 4 info bits in the frame
   c^=(1 & md.parity.p[lag][tsym[i]]);  //check control bit, result 0 is OK, 1 is parity error
   b+=(1&c); //add to errors counter
  }
  if(b>7) b=7;  //saturate to 7 errors maximum

  md.wfr.p[scnt]<<=3; //shift FIFO
  md.wfr.p[scnt]|=b;  //add lag errors to FIFO (contains last 10 frames)

  

  //search DC level of current symbol
  m=0; //DC level
  for(i=0;i<24;i++) m+=sptr[i]; //averages samples in the symbol
  m/=24; //set DC level

  //search position of maximal pulse relation DC in the symbol
  r=0; //max value
  k=0; //position of max pulse (0-23)
  for(i=0;i<24;i++) //check all samples
  {
   l=abs(m-sptr[i]); //absolute amplitude of sample over DC
   if(l>r) //if best
   {
    r=l; //save best apmlitude
    k=i; //save position
   }
  }
  
	//collect left and right uncentering of pulses (for adjusting sampling rate)
	if(k&&(k<23)) //only if pulse not in start or end of symbol
	{
	  al+=abs((int)sptr[k]-sptr[k-1]); //collect left shift 
    ar+=abs((int)sptr[k]-sptr[k+1]); //collect right shift
	}
	
	//---------------------------------------------------------------------
  //check is the current lag is exactly the 24 pcm symbol start
  if(lag!=(pbestlag%SAMPLESS_IN_SYMBOL)) continue;
  //---------------------------------------------------------------------
	
	//obtain channel polarity
	k/=3; //position of wave (3 pulses), must be even (or odd???)
	
	#ifdef MIRROR
	if(!(k&1)) pltcnt++;  //increment counter for wrong position
  else if(pltcnt) pltcnt--; //decrement to 0 for good position
  #else 
	if((k&1)) pltcnt++;  //increment counter for wrong position
  else if(pltcnt) pltcnt--; //decrement to 0 for good position
	#endif
	
  //set correct channel plarity
  #define POLTEST 10   //frames for change polarity
  if(pltcnt>(SYMBOLS_IN_FRAME*POLTEST))  //check level of wrong positions
  {
   plt^=1; //change polarity
   pltcnt=0; //reset counter
  }

 //---------------------------------------------------------------------
  //compute actual bit number in the real frame using value of best lag position
  bnum = (scnt-pbestlag)/SAMPLESS_IN_SYMBOL-1;  //actual triplet number minus one
  if(bnum<0) bnum+=SYMBOLS_IN_FRAME;   // ring    29
  bnum*=BITS_IN_SYMBOL; //actual bit number

  //compute LLR for symbol's bits
  for(i=0;i<POS_IN_SYMBOL;i++) rr[i]=rr[i]*15000/e; //normalize correlation coefficients

  SymbolLLR(0xFF, rr, md.fd.p+bnum); //compute soft bits from hard bits and normalized correlation coefficients

  //---------------------------------------------------------------------
  //check the current lag is exactly the new frame start
  if(scnt!=pbestlag) continue;

  //---------------------------------------------------------------------
  //now all 90 bits received and set on his places in fb ready to output

  //find average LLR in this frame
  if(fout)
  {
   m_s=0;
   for(i=0;i<BITS_IN_FRAME;i++) m_s+=abs(md.fd.p[i]); //sum LLR of all bits in frame
   m_s/=BITS_IN_FRAME; //average LLR for this frame
   if(!m_s) m_s=1; //must be non-zero for division
  }		       

  //clear  data for frame output processing
  memset(data, 0, 16); //clear bytes output array
  bnum=0; //init output bits counter
  ber=0;  //clear parity errors counter

  //---------------------------------------------------------------------
  //process all subframes in the frame
  for(i=0;i<SUBFRAMES_IN_FRAME;i++)  //18  process next subframe
  {
        c=0;   //clear subframe parity
        d=0x7FFF;  //set maximal possible metric value

        //process this subframe
        for(j=0;j<DATA_IN_SUBFRAME;j++)   //4  process all bits in subframe
        {
         k=md.fd.p[j*SUBFRAMES_IN_FRAME+i];  //18  set soft output bit value from input array
         if(fout) //optional soft output
	       {
	        z=abs((int)k*128/m_s);  //LLR normalized to unsigned char
	        if(z>255) z=255; //saturate
	        fout[bnum]=(unsigned char)z; //output normalized LLR
	       }
         if(k<0)  //hard bit decision: bit=1
         {
          data[bnum/8]^=mask[bnum%8]; //set hard output bit
          c^=1; //add bit to parity
         }
         if(abs(k)<d) //check level of metric of this bit: if lowest in this symbol
         {
          q=bnum; //remember number of bit with lowest metric
          d=abs(k); //set lowest metric level
         }
         bnum++; //next output bit
        } //end of processing all bits in subframe

       //---------------------------------------------------------------------
   //FEC
        if(md.fd.p[BITS_IN_FRAME-1-i]<0) c^=1; //check parity of processed subframe
        if(c&&(d<abs(md.fd.p[BITS_IN_FRAME-1-i]))) //if parity wrong and worse bit is not a parity bit
        {
         data[q/8]^=mask[q%8]; //flip corresponds hard output bit
         ber++;  //count bit error
        }

   } //end of processing all subframes in the frame

   //---------------------------------------------------------------------
   //compute parity error rate in percent
   if(pbit_err > BERBITS)  //check is total number of errors so large
   {
    pbit_err>>=1; //twice decrease total number of errors
    pbit_cnt>>=1; //twice decrease total number of received bits
    if(!pbit_cnt) pbit_cnt++;	//number of received bits must be non-zero (for division)
   }
   pbit_cnt+=BITS_IN_FRAME; //add number of received bits
   pbit_err+=ber; //add number of error
   z=(1000000/BITS_IN_FRAME)*pbit_err/pbit_cnt; //compute bit errors rate in percent*100
   if(z>4095) z=4095; //saturate to 12 bits
   
  //---------------------------------------------------------------------
   //search best lag value
   d=0; //set minimal possible level for search maximal value
   for(i=0;i<SAMPLES_IN_FRAME;i++) //look all lags (for every sample position in frame)
   {
    u=md.wfr.p[i];  //get parity errors FIFO value for this lag
    k=0; //weight of lag
    for(j=0;j<10;j++) //count for last 10 frames in fifo
    {
     k+=ltab[u&7]; //3 bits (0-7 errors) convert to log weight
     u>>=3; //set FIFO to next frame 
    }
    if(k>d) //check weight of this lag is the best
    {
     d=k; //set best weight
     q=i; //set best lag
    }
    //wwwtst[i]=k; //test
   }

  //---------------------------------------------------------------------
  //set carrier locked flag
  if(d>LOCK_TRS) //check best quality is sufficient
  {
   if(!plagflg) pbestlag=q; //if carrier was unlocked immediately set new modem lags
   plagflg=1; //set lag flag
  }
  else if(d<UNLOCK_TRS) plagflg=0; //if carrier poor clear lock flag

  //---------------------------------------------------------------------
  //check found lag is stable and set general lag
  u=plagseq;  //FIFO of best lag values for 4 last frames
  for(i=0;i<4;i++) //check for each is equal to best value of this frame
  {
   if((u&0xFF)!=b) break; //break if not equal
   u>>=8; //next value
  }
  if(i>=LAGSEQ) pbestlag=q;  //set current lag value as modem lag if sequence of last lags matched to current lag value
  plagseq = (plagseq<<8)+b; //add current lag value to FIFO for next

  //---------------------------------------------------------------------
  //output frame statistic
  data[11]=(ber<<1);	//raw BER counter (5 bit, 0-18)
  data[12]=(z>>4); //ber 8 MSB
  data[13]=(z&0x0F); //ber 4 LSB
	
	//current modem lag
  data[13]|=((pbestlag>>4)&0x30);  //4 MSB of lag
  data[14]=(pbestlag&0xFF); //8 LSB of lag (0-719)

  //flag of carrier locked
  if(plagflg) data[11]|=0x40; //block lag lock flag
 } //end of processed all samples
 
 //--------------------------------------------------------------------- 
 //compute value for fine sampling rate tuning
 #define CRATE 4 //coefficient for tuning timer  (for centering pulse to ADC probe)
 #define MRATE 0  //fixed shift of timer (constant difference of RX/TX samplin rates)
 m=ar+al; if(!m)m=0x7FFFFFFF; //compute divider for normalization (nonzero)
 peak=(CRATE*(ar-al)/m)+MRATE; //normalized difference between left and right samples of pulse
 ar=0; al=0; //clear summ for received frame
 data[15]=(unsigned char)(signed char)peak; //value for fine tuning recorded sample rate
 
 //---------------------------------------------------------------------
 //overlap frames to one symbol
 memcpy(md.tail.p, frame+SAMPLES_IN_FRAME-SAMPLESS_IN_SYMBOL, SAMPLESS_IN_SYMBOL*sizeof(short)); //copy last symbol to tail for next
 return peak;//returns sync value
}


