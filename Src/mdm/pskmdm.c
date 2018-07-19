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


//800 bps modem V3.3 fixed point for HW platforms  June 2018 


//This file contains procedures for modulate and demodulate data using pseudo-voice modem
//suitable for GSM-FR compressed channel
//This is BPSK modem with carrier parameters strongly adapted for GSM FR codec engine with VAD
//Some tricks are empirically discovered and can be not optimal. Now modem in development and can be
//improved with new ideas. Also code can be not optimal by performance and style due to continuous changes,
//Fixing point is used, optimized for  for 32 bits platform


#include "stdlib.h"
#include "math.h"
#include <string.h>
#include "modem.h"
#include "mdmdata.h"

//for 800 bps with 8KHz sampling 90 mS frame
//frame payload is 72 bits
//PSK: 1.5 period of 1333 Hz carrier / 1 bit

//carrier specified value
#define SAMPLESPERWAVE 3  //general for all bitrates for 48000 input sampling rate (3 8K samples corresponds GSM FR RPE engine is 1333Hz carrier)
//modulation specified value
#define WAVESPERBIT 3   //number of waves (carrier half-period) coded one bit. In the GSM compressed channel this value define modem redudance for narrowband
//frame format specified value
#define BITSPERSYMBOL 10   //number of raw bits per symbol
#define SYMBOLSPERFRAME 8  //number of symbols per frame
#define SYMBOLMASK 0xFF    //mask=2^SYMBOLSPERFRAME
//demodulator engine specified value
#define BITSPERSUBFRAME 4  //number of bits per demodulator subframe
//calculated values
#define SAMPLESPERBIT (SAMPLESPERWAVE*WAVESPERBIT) //number of 48KHz PCM samples per one raw data bit
#define DATAPERSYMBOL (BITSPERSYMBOL-1) //number of information bits per symbol (one is a parity bit)
#define BITSPERFRAME (SYMBOLSPERFRAME*BITSPERSYMBOL) //number of raw bits (data+parity) per frame
#define DATAPERFRAME (SYMBOLSPERFRAME*DATAPERSYMBOL) //numbers of data bits per frame
#define BYTESPERFRAME (DATAPERFRAME/8+1) //number of bytes in frame buffer
#define SUBFRAMESPERFRAME (BITSPERFRAME/BITSPERSUBFRAME) //numbers of demodulation subframes per frame
#define SAMPLESPERSUBFRAME (SAMPLESPERWAVE*WAVESPERBIT*BITSPERSUBFRAME) //samles processed by demodulator
#define SAMPLESPERFRAME (SAMPLESPERWAVE*WAVESPERBIT*BITSPERFRAME) //number of samples per frame

//***************************************************************************
//demodulator state (global values)
//***************************************************************************
//demodulator arrays
//unsigned short wsb[WAVESPERBIT][BITSPERSYMBOL]; //shifted parity bits for frame synchronizing
//unsigned int wfr[WAVESPERBIT][BITSPERSYMBOL*SYMBOLSPERFRAME];      //weights of lags
//short fd[BITSPERSYMBOL*SYMBOLSPERFRAME];      //metrics of bits
//short tail[SAMPLESPERWAVE*WAVESPERBIT]={0,}; //overlapped samples buffer
t_md md; //demodulator's state (shared with pulse modem)

static int cnt=0;  //counter of subframes in the frame
static int wlag=0; //lag of bit boundary
static int tlag=0; //lag of block (last bit position in the frame)

//lag searching values
static unsigned int lagseq=0; //sequence (FIFO) for four last quality values
static unsigned char lagflg=0; //flag of carrier locked

//counters for estimate parity errors rate will be near 5% for noise 
static unsigned int bit_cnt=1; //counter of receiver raw bits (must be non-zero for division)
static unsigned int bit_err=0; //counter ofparity bits errors (max is 8, noise is 4 errors to 80 raw bits)

//tone detector
static int qut=0; //tone quality level

//***************************************************************************
//modulator state (global values)
//***************************************************************************
static signed char lastb=1;		//value of the last transmitted bit for ISI
static signed char lastisi=1;    //value of last bit was shaped
static unsigned char lastfcnt=1; //last value of frame counter
static unsigned char llfcnt=1;   //last last value of frame counter
static unsigned char txcnt=0;  //conter of modulated subframes of the frame
static unsigned char fcnt=0;   //frame counter
static unsigned char txp[SYMBOLSPERFRAME]={0,};  //frame parity bits

//tone generator
static unsigned char tonecnt=0; //counter of modulated tone waves

//last sample of processed frame for resumpler
static int r_s;

//volatile int ttt; //test
//***************************************************************************
//tables
//***************************************************************************

//---------------------------------------------------------------------------
//modulator's waveform
//table: sin(30), 1, sin(30); -sin(30), -1, -sin(30)
 static const short wave[2][SAMPLESPERWAVE]=
 {
  {7000, 9000, 7000},  //shaped
  {4000, 8000, 4000} //normal
 };

//---------------------------------------------------------------------------
//wave table for 667 Hz tone
//table: 0, sin(30), sin(60)
//wave of 12 samples: 0, sin(30), sin(60), 1, sin(60), sin(30); 0, -sin(30), -sin(60), -1, -sin(60), -sin(30)
 static const int tone[SAMPLESPERWAVE]=
 {
    0, 8000, 13858
 };

//wave of 667 Hz (shaped)
//table: sin(15), sin(45), sin(75), sin(75), sin(45), sin(15)
static const short shape[2][SAMPLESPERWAVE]=
{
 {2071, 5657, 7727},
 {7727, 5657, 2071} 
};

//---------------------------------------------------------------------------

//bit mask
static const unsigned char mask[8]={1,2,4,8,16,32,64,128}; //bit mask table
//lag quality depends number of bits errors
static const unsigned char ltab[4]={4,2,1,0}; 

//##########################################################################
//############################ PROCEDURES ##################################
//##########################################################################

//***************************************************************************
//Tone 667Hz generator
//***************************************************************************
   //90 mS frame, 60 periods
   //modulate 'wcnt' waves of 667 Hz tone (12 8KHz samples per wave)
   unsigned char Tone(short* frame, unsigned char wcnt)
   {
    short n=0;
    short i,j;
    int q=1024;
    #define TONEMPL 1024
    //#define SOFTLEVEL


    //generate 60 periods SAMPLESPERWAVE*4 samples each
    for(i=0;i<wcnt;i++)
    {
#ifdef SOFTLEVEL   //soft change tone level frame by frame
     q=abs((tonecnt&127)-64);
     q=q*48+1024;
#elifdef FIXTLEVEL //step change tone level for even and odd frames
     if(tonecnt&64) q=4096; else q=1024; //apply anti-VAD
#else 
			q=4096;		//non change tone level	
#endif

     for(j=0;j<SAMPLESPERWAVE;j++) frame[n++]=tone[j]*TONEMPL/q; //0, 30, 60
     frame[n++]=16000*TONEMPL/q;                                 //1
     for(j=(SAMPLESPERWAVE-1);j>0;j--) frame[n++]=tone[j]*TONEMPL/q; //60, 30

     for(j=0;j<SAMPLESPERWAVE;j++) frame[n++]=-tone[j]*TONEMPL/q; //0, -30, -60
     frame[n++]=-16000*TONEMPL/q;                                 //-1
     for(j=(SAMPLESPERWAVE-1);j>0;j--) frame[n++]=-tone[j]*TONEMPL/q; //-60, -30

     tonecnt++;
    }

    return tonecnt;
   }

//***************************************************************************
//Tone 667Hz detector
//***************************************************************************
   short Detect(short* frame, unsigned char rptr)
   {
       //short* sp;  //pointer locked to start of processed subframe
       int i,j,k,d,q,p; //,n; //general int
       int peak=0; //energy lag
       int e[2*SAMPLESPERWAVE]; //energy array for estimate baseband quality
       int s=0; //summ of e
       
//=======================STREAM LOCKING=======================
//search max energy lag throughout LOOKUPWINDOW
//Search energy peak in range  of 0-2*SAMPLESPERWAVE
#define LOOKUPWINDOW1 24   //number of waves for forvard searching of energy lag
        //sp=frame; //set pointer to processed frame
        peak=0; //initial wave lag
        k=0;  //initiate for maximal delta summ
	for(i=0;i<2*SAMPLESPERWAVE;i++)  //search lag on each sample in one wave
        {
         d=0;  //summ of delta values for this lag
         j=0;  //wave will be checked
         while(j<LOOKUPWINDOW1) //summ deltas on a lookup window
         {
          //p=sp[j*2*SAMPLESPERWAVE+i]; //base value
          p=frame[(j*2*SAMPLESPERWAVE+i+rptr)&0xFF]; //base value
          j++;                      //to next wave
          //p-=sp[j*2*SAMPLESPERWAVE+i]; //difference
          p-=frame[(j*2*SAMPLESPERWAVE+i+rptr)&0xFF]; //difference
          d+=abs(p); //summ absolute differencies
         }
         e[i]=d; //for compute quality
         s+=d;
         if(d>k)//search largest value on each lag position of a wave
         {
          k=d;  //greatest value
          peak=i;   //max energy lag in range  0 - SAMPLESPERWAV
         }
        }

//=======================Obttain btone quality=========================
#define LOCKTH 23000 //level of tone is locked
#define UNLOCKTH 17000 //level of tone is unlocked

       //tone quality corresponds average ratio between central and side sample
       p=e[peak];  //centarl sample of the wave
       if(!peak) p+=e[2*SAMPLESPERWAVE-1]; else p+=e[peak-1];
       if(peak==(2*SAMPLESPERWAVE-1)) p+=e[0]; else p+=e[peak+1];
       q=s-p; //avarages side samples

       if(!q) q++;
       qut-=(qut>>7);  //averages baseband ratio
       qut+=((p<<7)/q); //add current baseband ratio to filter
       i=qut/256-64;

       if(i<0) i=0;
       if(i>255) i=255;
       return (short)i;
   }

//***************************************************************************
//Modulator
//***************************************************************************
   //SAMPLERATE 8000 KHz//
   //modulator: SYMBOLSPERFRAME=8 symbols DATAPERSYMBOL=9 payload bits each + parity bit to each symbol
   //interleaves symbols: transmit bits 0 of each symbol, then bits 1,... last parity bits (in reverse order)
   //For example:  DATAPERSYMBOL=9, SYMBOLSPERFRAME=8:  payload is 72 bits + 8 parity bits (one per each symbol) = 80 bits total
   //Bits: 0-9 and p, Symbols: A-H
   //Input data: A0,A1,A2,A3,A4,A5,A6,A7,A8,A9,B0,B1,B2,B3,B4....G8,G9,H0,H1,H2,H3,H4,H5,H6,H7,H8,H9
   //Output stream: A0,B0,C0,D0,E0,F0,G0,H0,A1,B1,C1,D1,E1,F1,G1,H1,A2,B2....A9,B9,C9,D9,E9,F9,G9,H9,Hp,Gp,Fp,Ep,Dp,Cp,Bp,Ap

   //One bit moduleated to WAVESPERBIT=3 waves containes SAMPLESPERWAVE=3 samples each
   //so each bit modulated to 3*3=9 samples (8KHz), and total number of samples is 720 per frame

	 //Anti-VAD trick is amlitudes of even frames are twice then amplitudes of add frames
	
   //Modulate DATAPERFRAME=72 bits (9 bytes) from 'data'  to SAMPLESPERFRAME=720 short PCM samples in 'frame'
   void Modulate_b(unsigned char* data, short* frame)
   {
    short i; //number of current symbol in the frame
    short j; //number of current bit in the symbol
    short k; //number of bit in the processed subframe
    short n; //sample counter

    signed char isi=0; //flag of controlled ISI
    short* sp=frame;    //pointer to PCM
    signed char b, bb; //bit value (1 or 0)
    short bn; //pointer to bit in the input data block
 
    //modulate bcnt bits with interleaving
    for(k=0;k<BITSPERFRAME;k++)
    { 
		 //number of ouput bit in frame
     j=txcnt%SYMBOLSPERFRAME; //source symbol for current output
     i=txcnt/SYMBOLSPERFRAME; //source bit for current output

     if(i!=DATAPERSYMBOL)  //this is data bit
     {
      bn=j*DATAPERSYMBOL+i; //number of requested input data bit
      if(data[bn/8]&mask[bn%8]) b=1; else b=0; //get data bit by number
      txp[j]^=b; //add this bit to parity of this symbol
     }
     else b=txp[(SYMBOLSPERFRAME-1)-j];  //this is parity bit for symbol (transmitt reverse)


     isi=b;   //swap current and last bits:
     b=lastb;  //last bit will be modulated now
     lastb=isi; //current bit will be modulated next later
     isi=b^lastb; //check for current bit is different last bit and set isi flag
     b=1-2*b; //modulated (last) bit to multiplier: 0/1 -> 1/-1
     bb=b;
     if(md.avad) //apply anti-vad trick
		 {
		  b+=(b*(lastfcnt&1));  //last vad to multiplier: 1/-1 or 2/-2 ('lastfcnt' for actually modulated 'lastb' bit)
      bb+=(bb*(llfcnt&1));  //last last vad to first wave
     }//now we was save current bit and will modulate previous bit
		 //each bit modulates to 3 1333 Hz waves of 3 samples each: first wave is normal, second is antipodal and third is normal  
		 //first and last waves can be shaped for phase discontinuous in the case phase of previous bit and next bit changed
		 
		 //this is normal wave 1
     //shaped if '!lastisi' (previosly modulated bit was the same of currently modulated)
     //amplitude set by last last bit
     if(!lastisi) for(n=0;n<3;n++)
     {
      sp[n]=bb*shape[1][n]; //shaped: a half of 666 Hz wave (down)
		 }
     else for(n=0;n<3;n++)
     {
      sp[n]=bb*wave[1][n]; //not shaped: 1333 Hz whole wave
     }
     sp+=3; //move pointer to next wave

     //this is antipodal wave 2: never be shaped
     //amplitude set by last bit
     for(n=0;n<3;n++)
     {
      sp[n]=-b*wave[1][n]; //not shaped, 1333 Hz whole wave
     }
     sp+=3; //move pointer to next wave

     //this is normal wave 3: shaped if '!isi' (curently modulated bit is the same with next modulated bit (saved just now to 'lastb')
     //amplitude set by last bit
     if(!isi) for(n=0;n<3;n++)
     {
      sp[n]=b*shape[0][n]; //shaped: a half of 666Hz wave (up)
     }
     else for(n=0;n<3;n++)
     {
      sp[n]=b*wave[1][n]; //not shaped: 1333 Hz whole wave
     }
     sp+=3; //move pointer to next wave

     
     lastisi=isi; //save isi of current bit for next (for shaping the first wave of next 'lastb' will be modulated)
     llfcnt=lastfcnt;
     lastfcnt=fcnt; //save frame counter (for vad trick value correspond to saved 'lastb')
     //check for whole frame was processed (all bits were extracted)
     txcnt++; //count subframes
     if(txcnt==BITSPERFRAME) //just extracted bit in 'lastb' was last bit of this frame
     {
      txcnt=0; //reset subframe counter
      memset(txp, 0, SYMBOLSPERFRAME); //init parity bits array for symbols of next frame
      fcnt++; //count frame
     }
    }
   }


//***************************************************************************
//Demodulator
//***************************************************************************

//=====================================
//coherent demodulator of BPSK 1333 Hz carrier
//for 800 bps stream splitted by 90ms blocks 72 bits payload each
//in-build FEC/sync is soft 9/10
//=====================================
//The goal of this code is low comlexity
//Input baseband signal not filtered and DC removed during processing

	//frame is SAMPLESPERFRAME=720 8KHz samples of resulting baseband carrie BITSPERFRAME=80 raw bits (payload+parity)
	//wave is a half-period of carrier contains SAMPLESPERWAVE=3 8KHz samples
	//bit is raw boolean data modulated with WAVESPERBIT=3 waves
	//symbol is small data block with DATAPERSYMBOL=9 payload bits and one parity bit (BITSPERSYMBOL=10 total)
	//a frame contain SYMBOLSPERFRAME=8 symbols
	//block is DATAPERFRAME=72 payload bits modulated to frame

   //input: 'frame' is SAMPLESPERFRAME=720 8 KHz samples
   //output: 'data' is DATAPERFRAME=72 bits (9 bytes) block is payload in data[0]-data[8] while ready flag (event) is set
   //'fout' is unsigned char array of DATAPERFRAME=72 metrics (LLR) of outputted bits 
   //7 MSB of data[10] is a frame aligning lag (0-79)
   //4 LSB of data[11] is a number of symbols parity errors in block (0-8, F for unprocessed frames)
   //MSBs of data[11] are:
   //bit 7 is a flag of payload ready (receiving event)
   //bit 6 is a flag of block synchronization locked (computed by parity error level)
   //bit 4-5 is a  phase lag  (0-2)

//returns: signed value (+-8) for fine tuning Timer divider for set recording sampling rate
//************************************************************************** 
	 short Demodulate_b(short* frame, unsigned char* data, unsigned char* fout)
	 {
	   //variables
     short peak=0; //energy lag
	   int bcnt=0; //counter  of bits in subframe
	   int vbn=0; //virtual number of bit in frame
	   int ber=0; //bit error
	   int bnum=0; //output bit number
     int w;  //phase lag: points to wave is a bit boundary
     int m_s; //average LLR, qality multiplier	
     
		 //arrays
     unsigned char wbit[3*WAVESPERBIT]; //hard bit values for each wave in a bit $$$$$
     int wllr[3*WAVESPERBIT]; //soft bit values for each wave in a bit  $$$$$
     
		 //general
		 int i,j,k,d,q,p,n,z,pj; //general int
		 unsigned int u; //general uint32_t
		 //unsigned char b; //general uint8_t
		 unsigned short s; //general uint16_t
		 
//all the processing are *NOT* requires DC elimination and energy normalization in baseband
//this is simplifies computation significantly

//STEP 1: provide think synch for fine sampling rate adjustment
//sum absolute difference between side samples (left, right) and middle sample.
//side samples must be symmetric over middle sample. This is possible in 2 cases:
//- middle sample is on peak of wave, and side samples are in +-60 degree from peak of this wave
//- middle sample is on DC, and side samples are in +-60 degree from DC-cross on two antipodal waves
//the second case is much better against phase jitter due GSM codec work so we use this case 
//We compute normalized difference between left and right samples (positive or negative) and
//output for fine sampling rate adjustment so this difference will be minimal. 

//-----------------------------------------------------------------------
//STEP 1: tune sampling rate (fine synchronization)
    k=0; d=0;
    for(i=0;i<240;i++) //process all waves in frame
    {
	   k+=(abs((int)frame[3*i]-frame[3*i+1])); //sum of l-m
	   d+=(abs((int)frame[3*i+2]-frame[3*i+1])); //sum of r-m
    }
    peak=8*(k-d)/(k+d); //output normalized l-r

//-----------------------------------------------------------------------
//STEP 2: bit detecting
		//LOOP: process all bits in the current frame
   //The detection engine use correlation of last two waves of bit 
   //Equalizing is not used due inefficient in GSM compressed (not AWGN) channel type
   //The waves already aligned so sample 1 (middle) is on DC, samples 0 and 2 (left and right) 
   //are on +-60 degree from DC cross on antipodal waves
   //So sample 2 and 3 will be on same wave on +-30 degree from wave peak
   //The correlation coefficient for this samples will be equal and we use 1, for middle sample (on DC) will be 0
   //This significantly simplifies correlation provided by sum only 
   //for each bit we correlate two last waves of bits skip first wave 0 because first wave have low 
   //quality after phase change. So we only sum two samples of wave 1 and sub two samples of wave 2  
 		
   //-------------------------------------BIT DETECTION-----------------------------------
   //we look for each wave in a bit and search best wave lag later using frame sync engine
   //this is a little complexed but more stable comparing separately locking to bit boundary  
	memset(data, 0, 12); //clear output buffer
  //======================================FRAME PROCESSING LOOP================================
	for(bcnt=0; bcnt<BITSPERFRAME; bcnt++) //bcnt is the bit counter in processed subframe
  {
	 //detect each bit as 3 values on 3 lags one wave each
	 //due think aligning the wave can be correlated on 2 points with same coefficient, third point is 0
	 if(!bcnt) //first bit saved in tail of last frame
   {
    wllr[0]=(int)md.tail.b[1]+md.tail.b[2]-md.tail.b[4]-md.tail.b[5];
    wllr[1]=(int)md.tail.b[2]+md.tail.b[3]-md.tail.b[5]-md.tail.b[6];
	  wllr[2]=(int)md.tail.b[3]+md.tail.b[4]-md.tail.b[6]-md.tail.b[7];
		 
		wllr[3]=(int)md.tail.b[4]+md.tail.b[5]-md.tail.b[7]-md.tail.b[8]; 
		wllr[4]=(int)md.tail.b[5]+md.tail.b[6]-md.tail.b[8]-frame[0];
		wllr[5]=(int)md.tail.b[6]+md.tail.b[7]-frame[0]-frame[1];
	  
		wllr[6]=(int)md.tail.b[7]+md.tail.b[8]-frame[1]-frame[2];
		wllr[7]=(int)md.tail.b[8]+frame[0]-frame[2]-frame[3];
		wllr[8]=frame[0]+frame[1]-frame[3]-frame[4];
   }
	 else //other bits processed from current frame
	 {
	  pj=bcnt*9-9;
	  
		wllr[0]=(int)frame[pj+1]+frame[pj+2]-frame[pj+4]-frame[pj+5]; 
		wllr[1]=(int)frame[pj+2]+frame[pj+3]-frame[pj+5]-frame[pj+6];
		wllr[2]=(int)frame[pj+3]+frame[pj+4]-frame[pj+6]-frame[pj+7]; 
	  
		wllr[3]=(int)frame[pj+4]+frame[pj+5]-frame[pj+7]-frame[pj+8];
		wllr[4]=(int)frame[pj+5]+frame[pj+6]-frame[pj+8]-frame[pj+9];
		wllr[5]=(int)frame[pj+6]+frame[pj+7]-frame[pj+9]-frame[pj+10]; 
	 
		wllr[6]=(int)frame[pj+7]+frame[pj+8]-frame[pj+10]-frame[pj+11]; 
		wllr[7]=(int)frame[pj+8]+frame[pj+9]-frame[pj+11]-frame[pj+12];
		wllr[8]=(int)frame[pj+9]+frame[pj+10]-frame[pj+12]-frame[pj+13]; 
	 }
	 
	 //hard decision of bits
	 if(wllr[0]>0) wbit[0]=1; else wbit[0]=0; 
	 if(wllr[1]>0) wbit[1]=1; else wbit[1]=0; 
	 if(wllr[2]>0) wbit[2]=1; else wbit[2]=0; 
	 
	 if(wllr[3]>0) wbit[3]=1; else wbit[3]=0; 
	 if(wllr[4]>0) wbit[4]=1; else wbit[4]=0; 
	 if(wllr[5]>0) wbit[5]=1; else wbit[5]=0; 
	 
	 if(wllr[6]>0) wbit[6]=1; else wbit[6]=0; 
	 if(wllr[7]>0) wbit[7]=1; else wbit[7]=0; 
	 if(wllr[8]>0) wbit[8]=1; else wbit[8]=0; 
	 
     //the result is hard decision of each wave in current bit in wbit and it's metric in wllr
     //Note: wllr is not normalized by stream energy, but we can compare soft values
     //as a posteriori probability because baseband level throughout one frame suppose stable
//------------------------------------------------------------------------------
     //Now we have a received bit and will be check parity of previously received data.
     //Each block contains SAMPLESPERFRAME=720 8KHz samples.
     //At a time we have previously received BITSPERFRAME=80 raw bits totally and split they to SYMBOLSPERFRAME=8 symbols BITSPERSYMBOL=10 raw bits each.
     //Each symbol contain DATAPERSYMBOL=9 payload bits and one parity bit
     //During each bit received we will be attempt to check parity of all previously received BITSPERFRAME=80 bits
     //assuming a current bit as a last bit in the block (frame boundary)
     //Computed parity word contains SUBFRAMESPERFRAME=8 bits of checksum probably be zero on frame boundary point. The result is the number of zero bits.  
     //So we averages this value separately in the all possible bit-aligned lags (BITSPERSYMBOL=10) positions after each bit received) during bit stream received
     //and can find the best lag probably pointed the position of the last bit in block (frame boundary)
     //This synchronization will be probably correct after only a few full blocks and we can
     //output the first correct block after near 270 mS of stream processed
     //starting at any time without any sync-sequences or other bit rate overheads

//-----------------------------------------------------------------------
//STEP 3: compute virtual bit and symbol number
		 //we have a free bit counter independent of frame aligning process: bcnt
     //this is a base for virtual bit numeration. Frame boundary will be obtained as a lag to this counter
     //Now we assumed than lag value already points to last bit of block
     //Using lag value and bit counter we can compute the actual number
     //of currently received bit in block and put received bit to its place in frame bit array

  //-------------------------------------BIT SAVING-----------------------------------
      vbn=cnt*BITSPERSUBFRAME+bcnt; //the virtual number of currently processed bit in the stream
      n=(vbn-tlag)-1; //actual current bit number in the frame
      if(n<0) n+=BITSPERFRAME; //over boundaries of frame
      md.fd.b[n]=wllr[wlag]>>4; //save soft bit to it's place in the output array (wlag= 0-8)

//-----------------------------------------------------------------------------------------
//STEP 4:  runtime parity computation for frame's boundary aligning
     //compute checksum supposedly current current wave is the last wave in current bit and bit is the last bit in the frame
     //Average the weight of this checksum (number of zero bits) in virtual bit position of wfr array 
     //after continuous stream receiving
     //the index of minimal value in wfr[] array corresponds the last wave of last bit in block
     //This checked at the momen of autput next full block
     //searching maximal value in wfr[] array and set this index as a frame lag value

 //-------------------------------------CHECK PARITY-----------------------------------
  //check parity for each wave lag separately. This allowed also discovering the best wave lag
  //while searching frame boundary by maximal averages parity value
  for(w=0; w<3*WAVESPERBIT; w++) //average parity for each wave in a bit
  {
	 d=wbit[w]; //hard decisions of current bit  (for current wave lag)
   k=0; //clear temporary parity word
	 for(i=0;i<BITSPERSYMBOL;i++)  //shift received bit over all symbols
	 {

	  md.parity.b[w][i]<<=1; //shift symbol
	  md.parity.b[w][i]|=d; //add new bit to symbol
    if(md.parity.b[w][i]&(SYMBOLMASK+1)) d=1; else d=0; //obtain old bit shifted out from symbol
    md.parity.b[w][i]&=SYMBOLMASK; //mask symbol
    k^=md.parity.b[w][i]; //add current symbol to parity word
	 }
         k^=md.parity.b[w][0]; //temporary remove last bits (parity) from word

         //reverse bit order of parity word
         d=0; //clear temporary reverse parity word
         for(i=0;i<SYMBOLSPERFRAME;i++) //process all bits
         {
          d<<=1; //shift left from original word
          if(k&1) d|=1; //add to reverse word
          k>>=1; //shift right reverse word
         }
         d^=md.parity.b[w][0]; //check validity of parity word for current state

         //compute the number of setted bits are parity errors
         k=0; //parity errors counter (up to 8 errors per frame)
         for(i=0;i<SYMBOLSPERFRAME;i++) //process all parity bits
         {
          if(d&1) k++; //add parity error
          d>>=1; //shift to next parity bit
         }
        
				 if(k>3) k=3; //saturate error number to 3
				 md.wfr.b[w][vbn] = (md.wfr.b[w][vbn]<<2)|k; //shift 16 last frames quality value and add number of errors in current frame
   }

//-------------------------------------------------------------------------------------
      //now we have bit array aligned to block boundaries with  soft  bits
      //note the block synchronization is invariant for channel inversion
      //but BPSK demodulation result is NOT invariant: the inversion of channel
      //cause the inversion of all payload bits in outputted block
      //So channel inversion is stable and depended the used physical media only
      //the software cam be look and set inversion flag later and invert full modem output continuously

      //Checks for recently received bit is the last bit in this frame
      //In this case we must process all previously received  BITSPERFRAME=80 bits
      //(all of them are in his places in fd[BITSPERFRAME=80] arrays) and clear array for receiving next block

//-----------------------------------------------------------------------
//STEP 5: check for recently received bit is the last bit in the frame and output whole frame
//-------------------------------------FRAME OUTPUT-----------------------------------
      //check for this was last bit in the frame - output whole frame
      if(vbn==tlag) //this was the last bit of packet: output packet
      {
       memset(data, 0, 12); //clear bytes output array
       bnum=0; //init output bits counter
       ber=0;  //clear BER counter

				//compute normalizing coefficients for soft bits
			 if(fout) //find average LLR in this frame
       {  
	      m_s=0;
	      for(i=0;i<BITSPERFRAME;i++) m_s+=abs(md.fd.b[i]); //sum LLR of all bits in frame
				m_s/=BITSPERFRAME; //average LLR for this frame
	      if(!m_s) m_s=1; //must be non-zero for division
			 }				 
				
       //process symbols in the frame
       for(i=0;i<SYMBOLSPERFRAME;i++)  //process all symbols in the frame
       {
        p=0;   //clear symbol parity 
        d=0x7FFFFFFF;  //set maximal possible metric value

        //process bits in the symbol
        for(j=0;j<DATAPERSYMBOL;j++)   //process all bits in every symbol
        {
         k=md.fd.b[j*SYMBOLSPERFRAME+i];  //set soft output bit  from input array
         if(fout) //optional soft output
		     {
		      z=abs(k*128/m_s);  //LLR normalized to unsigned char
		      if(z>255) z=255; //saturate
		      fout[bnum]=z; //output normalized LLR
		     }
         if(k>0)  //hard bit decision: bit=1
         {
          data[bnum/8]^=mask[bnum%8]; //set hard output bit
          p^=1; //add bit to parity
         }
         if(abs(k)<d) //check level of metric of this bit: if lowest in this symbol
         {
          q=bnum; //remember number of bit with lowest metric
          d=abs(k); //set lowest metric level
         }
         bnum++; //next output bit
        } //end of processing all bits in the symbol

//-----------------------------------------------------------------------
//STEP 6: FEC
        if(md.fd.b[BITSPERFRAME-1-i]>0) p^=1; //check parity of processed symbol
        if(p&&(d<abs(md.fd.b[BITSPERFRAME-1-i]))) //if parity wrong and worse bit is not a parity bit
        {
         data[q/8]^=mask[q%8]; //flip corresponds hard output bit
         ber++;  //count bit error
        }

       } //end of processing all symbols in the frame

//-----------------------------------------------------------------------
//STEP 7: set outputted flags and values
			 
			 //compute parity error rate in percents
       #define BERBITS  50 //number of error for twice decresing values       
			 
			 if(bit_err > BERBITS)  //check total number of errors is large
			 {
				bit_err>>=1; //twice decrease total number of errors
        bit_cnt>>=1; //twice decrease total number of received bits
        if(!bit_cnt) bit_cnt++;	//number of received bits must be non-zero (for division)			 
			 }
			 bit_cnt+=BITSPERFRAME; //add number of received bits
			 bit_err+=ber; //add number of errors
			 i=(1000000/BITSPERFRAME)*bit_err/bit_cnt; //compute bit errors rate in percents*100
			 if(i>4095) i=4095; //saturate to 12 bits
       data[12]=(i>>4); //ber 8 MSB
       data[13]=(i&0x0F); //ber 4 LSB
       data[11]=(ber<<1);	//raw BER counter (4 bit, 0-9)
       //current modem lag (0-719), 10 bits
       i=tlag*9+wlag; //tlag is bit lag in range 0-79 and walg is sample lag in range 0-8 
       data[13]|=((i>>4)&0x30);  //2 MSB of lag
       data[14]=(i&0xFF); //8 LSB of lag 
			 //Sampling rate correction value
       data[15]=(unsigned char)(signed char)peak;
			 //flag of carrier locked
			 if(lagflg) data[11]|=0x40; //block lag lock flag

//-----------------------------------------------------------------------
//STEP 8: on-the-fly inversion of outputted block depends lag
   //even if one-wave jump of lag occurred the polarity will be correct
   //this can be due occasion as the correlation on wave0-1 was better then wave1-2
   //so wave lag can be changed during work, and modem output will be inverted 
   //we must invert data back on the fly
   
	   i=(wlag/3)+3*tlag; //absolute modem lag (0-239)
	   if(i&1) for(i=0;i<9;i++) data[i]^=0xFF;	//invert output data for even laggs	 			

//--------------------------------------------------------------------------------
//STEP 9: search best lag (will be applied for next frame)	
   //correct lag: search best value for alignment to block boundaries
   //and bit boundaries
      
       d=0; //set minimal possible level for search maximal value
       for(w=0; w<3*WAVESPERBIT; w++)    //search for all waves in a bit
       {
        for(i=0;i<BITSPERFRAME;i++)  //search for all possible bit lags
        {
					//compute quality for this lag
					u=md.wfr.b[w][i]; //16 last frames quality value for processed lag
					k=0; //quality of this lag (0-64)
					for(j=0;j<16;j++) //count quality by errors in 16 frames
					{
					 k+=ltab[u&3]; //add quality coefficient for 0-3 errors
           u>>=2;	//go to next frame					
					}
					//check this quality is greater the others
					if(k>d) //the best lag in processed frame
					{
					 d=k; //remember this quality as a best
					 s=9*i+w; //remember this absolute lag as a best (0-239)
					}
					
        } //end of searching all possible bit lags
       } //end of searching lag	
    
#define LOCK_TRS 16 //quality level for set lock flag (1-64)
#define UNLOCK_TRS 16 //quality level for clear lock flag(0-63)
#define LAGSEQ 0			//number of sequentialy mautched lags for apply to modem (0-4) 
			  
			 //set carrier locked flag 
			 if(d>LOCK_TRS) //check best quality is sufficient 
			 {
				if(!lagflg) //if carrier was ulocked immediately set new modem lags
        {
         tlag=s/9; //set new modem bit lag
         wlag=s-(tlag*9); //set new modem wave lag
				}					
				lagflg=1; //set lag flag
			 } 
			 else if(d<UNLOCK_TRS) lagflg=0; //if carrier poor clear lock flag
			 
       //check found lag is stable and set general lag  			 
			 u=lagseq;  //FIFO of best lag values for 4 last frames
			 for(i=0;i<4;i++) //check for each is equal to best value of this frame
			 {
				if((u&0xFF)!=(s/3)) break; //break if not equal 
				u>>=8; //next value
			 }
			 
			 //set current lag value as modem lag if sequense of last lags matched to current lag value
			 if(i>=LAGSEQ)  
			 {
				tlag=s/9; //set new modem bit lag
        wlag=s-(tlag*9); //set new modem wave lag			 			 
			 }
			 lagseq = (lagseq<<8)+(s/3); //add current lag value to FIFO for next
			 
      } //end of output full block //if(j==lag)//
    } //end of process all bits of the frame //for(bcnt=0; bcnt<BITSPERFRAME; bcnt++) 
	
	  //Result is block and bit lag values based on the subframe counter and points to the lag of last bit in the frame 

//--------------------------------------------------------------------------------
//STEP 10: //copy last bit samples to overlapping buffer for processing at the start of next frame
     memcpy(md.tail.b, frame+(BITSPERFRAME-1)*WAVESPERBIT*SAMPLESPERWAVE, sizeof(short)*WAVESPERBIT*SAMPLESPERWAVE);
     //--------------------------------------------------------------------------------
    //returns value for adjusting record sampling rate
	  return peak;
		  
	 }
	  
//***************************************************************************
//Resumplers
//***************************************************************************
	
	 //resamle 540 6KHz speech PCM samples to 720 8KHz line PCM samples
	 void speech2line(short* sp, short* ln)
	 {
		 int i, s, d;
		 
		 for(i=0;i<180;i++) //process group of 3 speech samples produses 4 line samples
		 {
			s = *sp++; //speech 0
      *ln++ = (3*s+r_s)>>2; //line 0 is averages of stores speech -1 and speech 0			 
			 
			d = *sp++; //speech 1
			*ln++ = (s+d)>>1; //line 1 is averages of speech 0 and speech 1

      s = *sp++; //spech 2
      *ln++ = (3*d+s)>>2; //line 2 is averages of speech 1 and speech 2

      *ln++ = s; //line 3 is equal speech 2
      r_s = s;	 //save speech 2 as speech -1 for next		 
		 }
	 }
	 
	 //resample 720 8KHz line PCM samples to 540 6KHz speech PCM samples
	 void line2speech(short* ln, short* sp)
	 {
		 int i, s, d;
		 
		 for(i=0;i<180;i++) //process group of 4 line samples prodused 3 speech samples
		 {
      s=*ln++; //line 0
			
			d=*ln++; //line 1
      *sp++ = (2*s+d)/3;	//spech 0 is averages of line 0 and line 1		 
      
			s=*ln++; //line 2
      *sp++ = (2*s+d)/3; //spech 1 is averages of line 2 and line 1

      *sp++ = *ln++;	//spech 2 is equal line 3		 
		 }			 	 
	 }
	 
	 
	 void setavad(unsigned char val)
	 {
		 md.avad=val;
	 }

