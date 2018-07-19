
#include <inttypes.h>

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef uintptr_t uintptr;

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef intptr_t intptr;

// Note that it's important to define the unit8 as first union member, so that
// an array of uint8 may be used as initializer.
typedef union UN_256bitValue_
{
    uint8          as_uint8[32];
    uint16         as_uint16[16];
    uint32         as_uint32[8];
    uint64         as_uint64[4];
} UN_256bitValue;


typedef UN_256bitValue fe25519;


// ****************************************************
// Assembly functions. 
// ****************************************************

extern void	
fe25519_mul_asm(
    fe25519*       result,
    const fe25519* in1,
    const fe25519* in2
);
#define fe25519_mul fe25519_mul_asm
		
extern void
fe25519_square_asm(
    fe25519*       result,
    const fe25519* in
);
#define fe25519_square fe25519_square_asm	

extern void	
fe25519_add_asm(
    fe25519*       out,
    const fe25519* in1,
    const fe25519* in2
);
#define fe25519_add fe25519_add_asm			

extern void
	  fe25519_sub_asm(
    fe25519*       out,
    const fe25519* baseValue,
    const fe25519* valueToSubstract);	
#define fe25519_sub fe25519_sub_asm	
		
extern void	
	  fe25519_mpy121666add_asm(
    fe25519*       out,
    const fe25519* valueToAdd,
    const fe25519* valueToMpy);		
		
#define fe25519_mpy121666add fe25519_mpy121666add_asm	

// ****************************************************
// C functions. 
// ****************************************************		
		
		void
fe25519_pack(
    unsigned char out[32],
    volatile fe25519*      in
);
void
fe25519_unpack(
    volatile fe25519*            out,
    const unsigned char in[32]
);


#define crypto_scalarmult crypto_scalarmult_curve25519
#define crypto_scalarmult_base crypto_scalarmult_curve25519_base
#define crypto_scalarmult_BYTES crypto_scalarmult_curve25519_BYTES
#define crypto_scalarmult_SCALARBYTES crypto_scalarmult_curve25519_SCALARBYTES
#define crypto_scalarmult_curve25519_BYTES 32
#define crypto_scalarmult_curve25519_SCALARBYTES 32
extern int crypto_scalarmult_curve25519(unsigned char *,const unsigned char *,const unsigned char *);
extern int crypto_scalarmult_curve25519_base(unsigned char *,const unsigned char *);

extern void elligator2_isrt(uint8_t* p, uint8_t* r);
extern void fe25519_elligator2s_invert(
    fe25519*       r,
    const fe25519* x,
    fe25519*       t0,
    fe25519*       t1,
    fe25519*       t2,
		fe25519*       t3
);		
