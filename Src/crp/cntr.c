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


//this file contains initializing, control and crypto procedures for JackPair
//unsigned char testcrp(void) provide self-test of crypto engine, compares with test vector, returns 1 is OK, 0-failure
//short setrand(int res) accept ADC result and calls untill returns 1 (RNG ready flag) 
//void setkey(unsigned int pin) accept u32 pin-code and calls on the start of session for preparing fresh keypair
//void getrand(unsigned char* m, short len) generate len PRNG bytes to m
//unsigned char ike_ber(unsigned char* m, signed char* logout) in IKE mode write to logout text log of data statistic in m
//this procedure returns 1 if we haven't their key otherwise returns 0
//void work_ber(unsigned char* m, signed char* logout) in WORK mode optonally write to logout text log of data statistic in m

//GENERAL LOGIK:

//On IKE stage (work=0) TX:
//unsigned char txkey(unsigned char* m) calls for generating trasmitted data, returns flag of control packet  
//void make_pkt(unsigned char* m) calls followed for insert control blocks or whitening transmitted data
//void Modulate(unsigned char* data, short* frame) calls followed for modulate data to PCM frame of baseband

//On IKE stage (work=0) RX:
//short Demodulate( short* frame, unsigned char* data, signed char* fout) calls for demodulate recorded PCM frame to data and bit's metrics
//this procedure returns value for fine adjusting of recording samle rate 
//short check_pkt(unsigned char* m) calls followed for check block is a control or de-witening data
//this procedure returns flag of control or data packet was received
//unsigned char rxkey(unsigned char* m, unsigned char* sb) calls followed to process received data
//this procedure accumulate bit's mectrics and check crc of hard decission bits of their key
//if key was completely received provide DH and compute encrypting and decrypting symmetric keys
//returns mode flag (0- for still IKE and 1 for set work mode)

//On WORK stage (work=1) TX:
//unsigned char melpe_a(unsigned char* bits, short* inbuf) calls for encode speech frame to data, returns VAD flag (1-voice)
//void make_pkt(unsigned char* m) calls followed for insert control blocks on speech pauses or encrypt voice data
//void Modulate(unsigned char* data, short* frame) calls followed for modulate data to PCM frame of baseband

//On WORK stage (work=1) RX:
//short Demodulate( short* frame, unsigned char* data, 0) calls for demodulate recorded PCM frame to data
//this procedure returns value for fine adjusting of recording samle rate 
//short check_pkt(unsigned char* m) calls followed for check block is a control or decrypt voice data
//melpe_s(outbuf, bits) calls followed for voice data decode it to speech frame
//for control next speech frame filled by silency.

//-------------Control packet format----------------------------
//control packet carry information about 24-bits counter and has special format for possible diference from arbitrary data
//control packet  contain 3 bytes of header, 3 bytes of hight counter value and 3 bytes is low counter value
//header is goley23/12 encoded 12 first bits of crc32 of 3 bytes of counter. Bit 23 is always 0 for detect channel inversion
//hight counter value is golay23/12 encoded 12MSB of counter and xoring with crc32 of header with bit 23 is 1
//low counter value is golay23/12 encoded 12LSB of counter and xoring with crc32 of header with bit 23 is 0
//-------------Control packet detecting-------------------------
//for detecting of received packet is a control checks bit 23 of header and all bit inverted if need
//then bit 23 of header sets as 1, computes crc32 of header and xor with 3 bytes hight counter value
//resulted value decoded with golay23/12 to 12MSB of expected counter
//then bit 23 of header sets as 1, computes crc32 of headers and xor with 3 bytes low counter value
//resulted value decoded with golay23/12 to 12 LSB of expected counter
//then composed 24 bits counter and computed crc32 of 3 bytes of counter,
//12 first bits of this crc compared with golay 23/12 decoded header value
//if all 12 bits are equal the packet decissed as a control otherwise - as a data (voice in work mode or ked in IKE mode)
//for control packets we can synchrinize our counter with value transmitted in a control packet
//if delta is less then 0 (back) our counter is freezed (not incremented). If delta is +1 our counter is set to new value forverd
//if delta is more then 1 this delta is saved and compared with delta of next control. If it is the same our counter corrected to new arbitrary value forward.
//This is for preventing fake big correction forvard with imossibility to fast correction back 
//because our conter is strongly one-way and can't be set to arbitrary value back, only freezing current value and wait for TX.

 
#include "stdlib.h"
#include "math.h"
#include <string.h>
#include <stdio.h>

#include "crc32.h" //crc32
#include "golay23.h" //Golay
#include "shake.h" //Keccak
#include "../ecc/scalarmult.h" //x25519
#include "cntr.h"

//global values
static unsigned char mode=0; //0-IKE, 1 -work mode
static signed char pol=0;    //channel polarity
static int cnt_out=0; //counter of outgoing packets
int cnt_in=0;  //counter of incoming packets
static int dcnt=0;    //delta of actual and expected incoming counters

//for statistic log
unsigned int c_err=0; //absolute number of bad control packets
unsigned int c_bad=0; //rate of bad control packets
unsigned int c_all=1; //rate of all packets (must be non_zero for division)

unsigned int s_err=0; //absolute number of parity errors

unsigned int b_err=0; //absolute number of bit errors
unsigned int b_bad=0; //bit error rate
unsigned int b_all=1; //counter of data bits (must be non_zero for division)

//volatile int ttt1; //test

//data arrays
static unsigned char sid[16];  //sid for PRNG
static unsigned char secret[32]; //our secret key on IKE, encrypting/decrypring keys on work
static unsigned char our_key[36]; //our public key on IKE
static unsigned char their_key[36]; //elligator on start, their public key on IKE
static short accumulator[288]; //entropy collector on start, soft bits of their public key on ike

//constant data
static const unsigned char bitmask[8]={1,2,4,8,16,32,64,128}; //bit mask table
static const unsigned char bitset[16]={0,1,1,2, 1,2,2,3, 1,2,2,3, 2,3,3,4};

//helpers procedures prototypes
static inline unsigned int m2i(unsigned char* m, unsigned char p); //3 bytes array to int with polarity
static inline void i2m(unsigned int uu, unsigned char* m); //int to 3 bytes array
static inline unsigned int m2u(unsigned char* m); //4 bytes array to unsigned int
static inline void u2m(unsigned int uu, unsigned char* m); //unsigned int to 4 bytes array


//======================================Helpers=====================================

//portable convert 24 bit integer to 3 bytes unaligned array
static inline void i2m(unsigned int uu, unsigned char* m)
{
 m[0]=uu&0xFF;
 m[1]=(uu>>8)&0xFF;
 m[2]=(uu>>16)&0xFF;
}

//----------------------------------------------------------------------------------
//portable convert 3 bytes unaligned array to 24 bits integer with optional inversion
static inline unsigned int m2i(unsigned char* m, unsigned char p)
{
 unsigned int uu;

 uu=m[2];
 uu<<=8;
 uu|=m[1];
 uu<<=8;
 uu|=m[0];
 if(p) uu^=0xFFFFFF;
 return uu;
}

//----------------------------------------------------------------------------------
//portable convert 32 bit integer to 4 bytes unaligned array
static inline void u2m(unsigned int uu, unsigned char* m)
{
 m[0]=uu&0xFF;
 m[1]=(uu>>8)&0xFF;
 m[2]=(uu>>16)&0xFF;
 m[3]=(uu>>24)&0xFF;	
}

//----------------------------------------------------------------------------------
//portable convert 4 bytes unaligned array to 32 bits integer
static inline unsigned int m2u(unsigned char* m)
{
 unsigned int uu;

 uu=m[3];
 uu<<=8;
 uu|=m[2];
 uu<<=8;
 uu|=m[1];
 uu<<=8;
 uu|=m[0];
 return uu;
}


//=====================================Packets processing===================================

//make 9 bytes outgoing packet from voice data (m[9] is VAD flag)
void make_pkt(unsigned char* m)
{
 unsigned int u; //intermediate value
 if(!m[9])  //no VAD: this is silency
 {          //make control packet
  u=crc32((unsigned char*)&cnt_out, 3); //set header as an crc of current counter
  u=golay23_encode(u&0xFFF); //encode
  i2m(u, m); //convert to array

  u=golay23_encode((cnt_out>>12)&0xFFF);  //encode hight bits of counter
  m[2]|=0x80;  //set MSB of header
  u^=crc32(m, 3); //mask 1
  i2m(u&0xFFFFFF, m+3); //convert to array

  u=golay23_encode(cnt_out&0xFFF); //encode low bits of counter
  m[2]&=0x7F;  //clear MSB of header
  u^=crc32(m, 3);  //mask 0
  i2m(u&0xFFFFFF, m+6);  //convert to array
 }
 else  //voice active
 {  //encrypt voice data
  sh_ini();  //initialize shake
  sh_upd(&cnt_out, 3); //absorb counter
  if(mode) sh_upd(secret, 16);  //absorb e-key (work mode only)
  sh_xof(); //permute to gamma
  sh_crp(m, 9); //xor data with gamma
 }
 cnt_out++; //increment outgoing packet counter
}
 
//----------------------------------------------------------------------------------
//process 9 bytes incoming packet to voice data, returns 1 for voice, 0 for silency
short check_pkt(unsigned char* m)
{
 unsigned char p=(m[2]&0x80); //expected channel polarity
 unsigned char mm[10]; //temporary array
 unsigned int u; //intermediate value 
 int r=0;  //result

 cnt_in++;  //next input packet

 if(!(m[11]&0x40)) return 1; //if no carrier returns data flag
	
 u=m2i(m, p);  //convert header to int
 u=golay23_decode(u&0x7FFFFF); //decode header
 i2m(u, mm); //convert header to array

 mm[2]|=0x80;    //set header's MSB
 u=m2i(m+3, p);  //convert hight data bits to int
 u^=crc32(mm, 3); //unmask 1
 u=golay23_decode(u&0x7FFFFF)>>11; //decode
 r=(u&0xFFF)<<12; //set MSB of expected counter

 mm[2]&=0x7F;  //clear header's MSB
 u=m2i(m+6, p); //convert low data bits to int
 u^=crc32(mm,3); //unmask
 u=golay23_decode(u&0x7FFFFF)>>11; //decode
 r|=(u&0xFFF); //add LSB to expected counter

 u=crc32((unsigned char*)&r, 3); //set expected header as an crc of expected counter
 u=golay23_encode(u&0xFFF); //encode
 u^=m2i(mm, 0); //check header is valid (for control packets only)

 if(!u)  //this is control packet
 { 
  //try correct actual counter of incoming packets by expected value	
	r-=cnt_in; //delta with current local counter and expected value
	
	if(!mode)  //in IKE mode: can correct to any value in both directin
	{
	 cnt_in+=r; //correct current counter
	 dcnt=0; //clear current delta
   pol=p>>7; //set global channel polarity		
	}
	else if((r==dcnt)||(r<2)) //in work mode: correct only if delta is equal saved delta or less then 2
  {
	 if(r<0) r=-1; //counter can back only by 1 (actually freeze)
   cnt_in+=r; //correct current counter
   dcnt-=r; //correct current delta
	 pol=p>>7; //set global channel polarity	
  }
  else dcnt=r; //delta too large (may be falce detecting of control packet): only save delta
	m[11]|=0x80; //set sync flag 
	return 0;  //returns control flag	
 }
 else //this is data packet: decrypt it
 {
  sh_ini(); //init shake
  sh_upd(&cnt_in, 3); //absorb incoming counter
  if(mode) sh_upd(secret+16, 16); //absorb decrypting key (work mode only)
	sh_xof(); //permute to gamma
  sh_crp(m, 9); //xor data with gamma
	if(pol) for(r=0;r<9;r++) m[r]^=0xFF; //apply channel polarity
  return 1; //return data flag
 }

}

//=====================================Key processing===================================

//check cnt_out and output control or key data to m
//returns not_control flag
unsigned char txkey(unsigned char* m)
{
 unsigned char* p; //pointer to output data

 m[9]=cnt_out%5; //set not_control flag: transmit one control packet followed 4 data packets	
 if(m[9]) //make data packet
 {
  p=our_key+9*(m[9]-1); //pointer to output data
  memcpy(m,p,9); //output 9 bytes of our key
 }	 
 return m[9];	//returns not_control flag
}

//----------------------------------------------------------------------------------
//check cnt_in, update soft bits in accumulator, check crc and/or change mode
//input: 72 hard bits in a byte array, 72 metrics in a byte array
//returns: current mode (0-ike, 1-work)
unsigned char rxkey(unsigned char* m, unsigned char* sb)
{
	short i=cnt_in%5; //packet type (0:control, 1-4:parts of key)
	unsigned char lk=m[11]&0x40; //carrier lock flag
	signed char k; //multiplier depends frame quality
	int bn; //number of first bit of received data
	unsigned int u; //checksumm
	short* p; //pointer to soft bits in accumulator
	
	//check data will be processed
  if(cnt_in>0x80FFFF) return mode; //preventing of fail set work mode	
	if((cnt_in&0x800000)&&(cnt_out&0x800000)) mode=1; //set work mode if both sides have keys
	if(!i||(cnt_out&0x800000)) return mode; //not process data for control packets and  if we already have their key
	if(!lk) //if carrier not locket by demodulator
	{
		memset(accumulator, 0, sizeof(accumulator)); //clear accumulator
		return mode; //not process noise data
	}
	
	//obtaing pointers to part of key will be processed
	bn=72*(i-1); //number of first bit of received data
	p=accumulator+bn; //pointer to soft bits in accumulator
	
	//set frame quality
	k=(m[11]>>1)&0x0F; //number of bits errors in received data block
	//this can be 0-7; for random data will be near 3-4, those block is not usefull, multiplier will be 0
	//0,1,2 converts to multipliers 16,4,1
	if(!k) k=16;
	else if(k==1) k=4;
	else if(k==2) k=1;
	else k=0;
	
	//accumulate soft bits with hard decission
	for(i=0;i<72;i++)
	{
		if(m[i>>3]&bitmask[i&7]) p[i]+=(short)sb[i]*k; //add received soft bit 1 to accumulator
		else p[i]-=(short)sb[i]*k; //add received soft bit 0 to accumulator
		p[i]-=(p[i]>>3); //accumulate filtering: k=2^3=8
		if(p[i]>0) their_key[bn>>3]|=bitmask[bn&7]; //set hard bit 
		else their_key[bn>>3]&=~bitmask[bn&7]; //or clear hard bit
		bn++; //process next received bit
	}
	
	//check crc32 of whole their key
	u=crc32(their_key, 32); //compute crc32 of received key
	u^=m2u(their_key+32); //check crc32
	
	//process received valid their key
	if(!u)   
	{	
		//compare our and their keys to obtain role
		k=0; //default our role
		for(i=0;i<32;i++)  //compare keys byte-by-byte
		{
			if(their_key[i]>our_key[i]) //if next byte of their key greater then in our_key
			{
			 k^=0xFF; //invert our role if their key large
			 break; //finishe comparing
			}
			else if(their_key[i]<our_key[i]) break; //else if next byte of their key smoler then in our_key: finishe with unchanged role
		}
		
		//check their and our keys are not equal and perform DH 
		if(i!=32) //check at least one byte were not equal during previous comparing of our and their keys
		{	
		 //compute DH secret to their_key
		 crypto_scalarmult_curve25519((unsigned char*)accumulator, secret, their_key); //Diffie-Hellmann
		 
		 //compute encrypting key
		 sh_ini(); //init shake
     sh_upd(&k, 1); //absorb role
     sh_upd((unsigned char*)accumulator, 32); //absorb DH secret
	   sh_xof(); //permute 
     sh_out(secret, 16); //output encrypting key
		 
		//compute decrypting key
		 k^=0xFF; //set their role
		 sh_ini(); //init shake
     sh_upd(&k, 1); //absorb role
     sh_upd((unsigned char*)accumulator, 32); //absorb DH secret
	   sh_xof(); //permute 
     sh_out(secret+16, 16); //output decrypting key	
		
		 memset(accumulator, 0, sizeof(accumulator)); 	//clear accumulator
		 //set outgoing counter indicates we have their key	
		 cnt_out=0x800000; 
		 } //end of: their and our keys are not equal 
		
		} //end of: process received valid their key
	
	return mode; //returns current mode (0-IKE, 1-work)
}

//----------------------------------------------------------------------------------
//set new key pair for SPECE protocol autenticated by pin
void setkey(unsigned int pin)
{
	unsigned int u;
	
	#define JP_SALT (const char*)"jp1_salt"  //constant application-depends salt for hashing pin code
	
	//hash pin with salt to 'our_key' array
	sh_ini(); //init shake
	sh_upd(JP_SALT, 8); //absorb salt
  sh_upd(&pin, 4); //absorb pin 
	sh_xof(); //permute 
  sh_out(our_key, 32); //output authenticator
	
	//compute base point for DH into 'their_key' array using ellegator2 algo
	elligator2_isrt(their_key, our_key); //hash2point on curve25519
	
  //generate random private key to 'secret' array
	getrand(secret, 32);
	
	//compute our public key to 'our_key' array
	crypto_scalarmult_curve25519(our_key, secret, their_key);
	
	//compute crc32 of our public key
	u=crc32(our_key, 32); //compute crc32 of our public key
  u2m(u, our_key+32); //convert crc32 to last 4 bytes of our_key array
	//clear rx arrays
	memset(their_key, 0, sizeof(their_key));
	memset(accumulator, 0, sizeof(accumulator));
}


//=====================================PRNG==========================================

//add one bit of entropy to collector
//input: right-aligned adc value
//returns: ready flag indicates sid is ready
short setrand(unsigned int res)
{
  #define RNDBYTES 512 //bytes in entropy collector

	int i; //general
	unsigned char* p=(unsigned char*)accumulator; //pointer to collector byte array
	unsigned short min, max=0; //statistic of collected entropy
 
	if(res==dcnt) cnt_in++; //adc value not changed: count cycles
  else	//adc value was changed
  {
	 dcnt=res; //set current value to compare for next
	 res^=cnt_in; //set last bit is entropy of value and time to change
	 cnt_in=0; //clear change time
	 if(res&1) p[cnt_out>>3]^=bitmask[cnt_out&7]; //add one bit to entropy
	 cnt_out++; //count entropy bits 
	 
	 if(cnt_out>RNDBYTES*8) //if whole collector was renewed
	 {
		cnt_out=0; //clear bit counter
		for(i=0;i<16;i++) sid[i]=0; //clear statistic area
		for(i=0;i<RNDBYTES;i++) //obtain statistic of collected entropy
		{
		 sid[p[i]&15]++;	//count variants 0-F of low nibble of entropy byte
		 sid[p[i]>>4]++;	//count variants 0-F of hight nibble of entropy byte
		}
		
		min=0xFFFF; //set min level to maximal possible value
		for(i=0;i<16;i++) //search minimal and maximal staticstic levels of counted nubble variants
		{
		 if(sid[i]<min) min=sid[i];
     if(sid[i]>max) max=sid[i];			
		}
		
		if((min>0x28)&&(max<0x58)) //check is entropy statistic is in acceptable range
		{
		 //hash entropy to sid
	   sh_ini(); //init shake
     sh_upd(p, RNDBYTES); //absorb pin 
	   sh_xof(); //permute 
     sh_out(sid, 16); //output authenticator
     max=1; //set ready flag			
		}
		else max=0; //entropy not good yet
	}		 
 }
 return max; //return ready flag
}

//----------------------------------------------------------------------------------
//output len PRF bytes
void getrand(unsigned char* m, short len)
{
 sh_ini(); //init shake
 sh_upd(sid, 16); //absorb sid 
 sh_xof(); //permute 
 sh_crp(sid, 16); //renew sid
 sh_out(m, len); //output PRF	
}

//self-test of assemler cryptographic procedures (x25519_mult, x25519_elligator2, keccak-800 sponge)
//returns 1 is OK, 0 is failure
unsigned char testcrp(void)
{
 #define TST_VECTOR 0xAF91E972
 unsigned int i;
	
 memset(our_key, 0x55, 32); //set initial value
 for(i=0;i<16;i++) //provide sequence of computing
 {	//secret=H1(value); value^=H2(value); their_key=elligator2(value); value = their_key^secret; 
  sh_ini(); //init shake	
  sh_upd(&i, 4); //absorb counter
  sh_upd(our_key, 32); //absorb value
  sh_xof(); //permute 	
  sh_crp(our_key, 32); //update initial value	
  sh_out(secret, 32); //output secret key
  elligator2_isrt(their_key, our_key); //compute curve point into their_key
  crypto_scalarmult_curve25519(our_key, secret, their_key); //compute public key		
 }
 //compare crc32 of resulting value with precomputed vector
 if(TST_VECTOR ^ crc32(our_key, 32)) return 0; //result fail
 else return 1; //result ok
}

//Statistic in IKE mode
//input: received data in m, this is a data packet if data_flag=0
//output: printf statistic report to logout
//returns: flag of we haven't their key
unsigned char ike_ber(unsigned char* m, signed char* logout)
{
 #define ERRRATE  50   //total counter level for twice reduction in percent rate calculation
 
 int i;
 unsigned char* p;	//pointer to temporary buffer
 unsigned char b, e; //byte value, error counter
 //outputted rate values in decimal format	
 unsigned char cer_h; //percents of control packets errors
 unsigned char cer_l; //centiles
 unsigned char ser_h; //percents of parity bits errors
 unsigned char ser_l; //centiles
 unsigned char ber_h; //percents data bits errors
 unsigned char ber_l; //centiles
 //statistic values
 unsigned short pcm_lag;  //lag of pcm samples processed by modem
 unsigned char lock_flag; //flag of carrier locked
 signed char rate_tune; //valur of tune timer for fine sync 	
 unsigned char sync_flag=(unsigned char)!m[11]&0x80; //1-sync, 0-data
 unsigned char key_flag=(unsigned char)!(cnt_out&0x800000);
 int pkt_id=0; //packet id for good packet or 0
 //------------------------------------------------------------	
 //process key data only we have their key 
 if(cnt_out&0x800000)
 {
  //------------------------------------------------------------
  //check for this packet must be a control by counter, set pointer to expected data
  i=cnt_in%5;
  if(!i) //expected control packet
  {
   if(sync_flag) //actally this packet was detected as a data, not a control
	 {
	  c_err++; //add control packets error to absolute counter
	  c_bad++; //add control packets error to rate
	 }
	 p=(unsigned char*)accumulator; //set pointer to temorary buffer
	 i=cnt_out; //save current cnt_out value
	 cnt_out=cnt_in; //set cnt_out value to expected cnt_in value
	 make_pkt(p); //make control packet for expected cnt_in value
	 cnt_out=i; //restore current cnt_out value
	 if(pol) for(i=0;i<9;i++) p[i]^=0xFF;
  } //expected data packet:
  else p=their_key+9*(i-1); //pointer to part of their key expected in m 
  //------------------------------------------------------------
  //count control packet error rate
  c_all++; //add all packets rate
  if(c_bad>ERRRATE) //check rate of control packets error is large
  {
	 c_bad>>=1; //twice decrease rate of control packets errors
	 c_all>>=1;  //twice decrese rate of data packets
	 if(!c_all) c_all++; //must be non-zero for division
  }
//------------------------------------------------------------ 
  //count bit errors
  e=0;
  for(i=0;i<9;i++) //count bit errors in each byte
  {
	 b=m[i]^p[i]; //compare bytes 
	 e+=bitset[b&0xF]; //count errors in low nibble
   e+=bitset[b>>4];	//count errors in hight niblle 
  }
	
  if(b_bad>ERRRATE) //check rate of bits error is large
  {
	 b_bad>>=1;  //twice decrease rate of bit errors
	 b_all>>=1;  //twice decrese rate of data bits
	 if(!b_all) b_all++; //must be non-zero for division
  }
  b_err+=e;   //add errors to absolute counter
  b_bad+=e;   //add bit errors to rate
  b_all+=72;  //add number of data bits
  if(e<20) pkt_id=cnt_in; //else pkt_id=0; //set packet id to current cnt_in value or set 0 for bad packets
 }
 
//-----------------------------------------------------------
 //obtain lag and flags
 pcm_lag=m[13]&0x30; //2 MSB of lag 
 pcm_lag=(pcm_lag<<4)+m[14]; //8 LSB of lag
 lock_flag=!(!(m[11]&0x40));	//flag of carrier locked
 rate_tune = (signed char)m[15]; //sampling rate tuning value (+-127)
 
 //-----------------------------------------------------------
 //total parity errors (0-18 for PLS, 0-8 for PSK)
 s_err+=(0x1F&(m[11]>>1)); //add frame parity errors to total counter
 
 //BER in %: up to 40.95%
 i=m[12]; //8 MSB of BER 
 i<<=4; //put to place
 i+=(m[13]&0x0F); //4 LSB of BER
 
 ser_h=i/100; //0-40
 ser_l=i-100*ser_h; //00-99
 //packet lost
 i=(int)c_bad*50000/c_all;
 cer_h=i/100;
 cer_l=i-100*cer_h;
 //bit errors 
 i=(int)b_bad*10000/b_all;
 ber_h=i/100;
 ber_l=i-100*ber_h;
 
//----------------------------------------------------------- 
 //output statistic:
 sprintf((char*)logout, "D=%d P=%d.%02d(%d) S=%d.%02d(%d) B=%d.%02d(%d) L=%002d C=%d/%d/%d\r\n",	
 //received packet number sequence
  pkt_id, //packet number for good packets or 0 for bad
	//rate of undetected control packets
  cer_h, cer_l, //control packets error in %
	c_err, //control packets errors absolute count
	//parity errors rate
  //ser_ppm, //parity error in %
  ser_h, ser_l,
	s_err, //parity errors absolute count
	//bit error rate
  ber_h, ber_l, //bit error in %
	b_err, //bit errors absolute count
	//frame boundary lag
  pcm_lag, //PCM lag in samples (0-719)
	//flags
  lock_flag, //1-modem locked (received data probably valid)
	pol, //channel polarity (1-phisical inversion)
  rate_tune); //+=8: value for fine tune recording samplerate 
 
 logout[255]=0; //terminate output for safe
 return (unsigned char)!(cnt_out&0x800000); //flag of we haven't their key
}

//Statistic in work mode
//input: received data in m, this is a data packet if data_flag=0
//output: printf statistic report to logout
void work_ber(unsigned char* m, signed char* logout)
{
 int i;
 signed char ser_h; //percents of parity bits errors
 signed char ser_l; //centiles
 char rxc, txc; //control char for RX and TX statuses
 
 unsigned short pcm_lag; //lag of PCM stream, processed by modem (in samples: 0-719)
 unsigned char lock_flag=(unsigned char)!(!(m[11]&0x40)); //flag of carrier locked (1-lock, 0-no carrier)
 signed char rate_tune=(signed char)m[15]; //value of tune timer for fine sync
 unsigned char sync_flag=(unsigned char)!(!(m[11]&0x80)); //1-sync, 0-data	
 unsigned char btn_flag=m[13]&0x80; //press button flag (1-pressed)
 unsigned char vad_flag=m[13]&0x40; //vad flag (1-speech)	
 
	
 //obtain lag and flags
 btn_flag =(unsigned char)!(!(m[13]&0x80)); //press button flag (1-pressed)
 vad_flag =(unsigned char)!(!(m[13]&0x40)); //vad flag (1-speech)	
 sync_flag=(unsigned char)!(!(m[11]&0x80)); //1-sync, 0-data	
 lock_flag=(unsigned char)!(!(m[11]&0x40));	//flag of carrier locked
 
 pcm_lag=m[13]&0x30; //2 MSB of lag 
 pcm_lag=(pcm_lag<<4)+m[14]; //8 LSB of lag
 
 rate_tune = (signed char)m[15]; //sampling rate tuning value (+-127)	
	
 //total parity errors (0-18 for PLS, 0-8 for PSK)
 s_err+=(0x1F&(m[11]>>1)); //add frame parity errors to total counter
 
 //BER in %: up to 40.95%
 i=m[12]; //8 MSB of BER 
 i<<=4; //put to place
 i+=(m[13]&0x0F); //4 LSB of BER
 ser_h=i/100; //0-40
 ser_l=i-100*ser_h; //00-99
 
 //set incoming packet type
 if(!lock_flag) rxc='X'; //noise
 else if(sync_flag) rxc='C'; //control
 else rxc='V'; //voice

 //set outgoing packet type
 if(!btn_flag) txc='X'; //button not pressed: native voice outputted to Line
 else if(!vad_flag) txc='C'; //no speech: control data outputted to Line
 else txc='V'; //voice data outputted to line
 //----------------------------------------------------------- 
 //output statistic:
 sprintf((char*)logout, "D=%d %c>%c S=%d.%02d(%d) L=%002d C=%d/%d/%d\r\n",	
 //received packet number sequence
  cnt_in, //packet counter 
	//packet's type
  rxc, txc,
  //parity error in % and absolute
  ser_h, ser_l, s_err, 
	//frame boundary lag
  pcm_lag,//PCM lag (0-719) 
	//flags
  lock_flag, //1-modem locked (received data probably valid)
	pol, //channel polarity (1-phisical inversion)
  rate_tune); //+=8: value for fine tune recording samplerate 
//-----------------------------------------------------------  
 logout[255]=0; //terminate output for safe
}

