/*                          =======================
  ============================ C/C++ HEADER FILE =============================
                            =======================                      

    \file fe25519_elligator2_isrt.c

    Variant of fe25519_elligator2.c that uses the "inverse square root trick".
    Compute elligator2 using a hint of Mike Hamburg that allows
    to calculate the inverse of 1+u.r^2 and the Legendre symbol epsilon during
    a single operation.
 
    \Author: B. Labrique, Endress + Hauser Conducta GmbH & Co. KG

    License: CC0 1.0 (http://creativecommons.org/publicdomain/zero/1.0/legalcode)
  ============================================================================*/
//#include "../local_includes/fe25519.h"
//#include "../local_includes/simple_randombytes.h"
#include <string.h>
#include "scalarmult.h"
#include "fe25519_pow2523.h"

const fe25519 fe25519_zero = {
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,   
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};
	
const fe25519 fe25519_one = {
	1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,   
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};

const fe25519 fe25519_A = {
	0x06,0x6D,0x07,0x00, 0,0,0,0, 0,0,0,0, 0,0,0,0,   
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};

const fe25519 fe25519_minusA = {
	0xE7,0x92,0xF8,0xFF, 0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,   
  0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0x7F
};


const fe25519 fe25519_minusAdiv2 = {
	0x6A,0x49,0xFC,0xFF, 0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,   
  0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0x7F
};

const fe25519 fe25519_Asquare = {
	0x24,0x1C,0xC2,0x24, 0x37,0,0,0, 0,0,0,0, 0,0,0,0,   
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};

// s = (c.d^2)^((p-3)/2)
void
fe25519_elligator2s_useProvidedScratchBuffers(
    fe25519*       r,
    const fe25519* x,
    fe25519*       t0,
    fe25519*       t1,
    fe25519*       t2
)
{
    fe25519_pow2523_useProvidedScratchBuffers(r, x, t0, t1, t2);
    // r is now x ^ (2^252 - 3)
    fe25519_square(r, r);
    // r is now x ^ (2^253 - 6)
    fe25519_square(r, r);
    // r is now x ^ (2^254 - 12)
    fe25519_mul(r, r, x);
    // r is now x ^ (2^254 - 11)
}



// s = (c.d^2)^((p-3)/2)


void
fe25519_elligator2_isrt_useProvidedScratchBuffers(
    fe25519*       x,
    const fe25519* r,
    // Scratch buffers
    fe25519*       t0,
    fe25519*       t1,
    fe25519*       t2,
    fe25519*       t3,
    fe25519*       v
    )
{
    fe25519* d = v;
    fe25519* a = t2;
    fe25519* b = t0;
    fe25519* c = t3;
    fe25519* s = x;
    fe25519* eps = t0;

    // dead: x, t0, t1, t2, t3, v
    fe25519_square(d, r); // d = [r]^2
    fe25519_add(t0, d, d); // t0 = [r^2] + [r^2]
    fe25519_add(d, t0, &fe25519_one); // d = 1 + [2.r^2]

    // dead: x, t1, t2, t3
    fe25519_mul(t2, &fe25519_Asquare, t0); // t2 = [A^2] * [2.r^2]
    // dead: x, t0, t1, t3
    fe25519_square(t1, d); // t1 = [d]^2
    // dead: x, t0, t3
    fe25519_sub(t0, t1, t2); // [d^2] - [(A^2).2.r^2]
    // dead: x, t2, t3
    fe25519_mul(a, t0, &fe25519_minusA); // a = [-A] * [(d^2 - (A^2).2.r^2)]
    // dead: x, t0, t3
    fe25519_mul(b, v, t1); // b = [d] * [d^2] = (1 + 2.r^2)^3
    // dead: x, t1, t3

    fe25519_mul(c, a, b); // c = [a] * [b]
    // dead: x, t0, t1, t2

    // s = (c.d^2)^((p-3)/2) with p = 2^255 - 19 -> (p-3)/2 = 2^254 - 11
    fe25519_mul(t2, c, d); // t2 = [c] * [d]
    // dead: x, t0, t1, t3
    fe25519_mul(v, d, t2); // v = c.d^2 = [c.d] * [d]
    // dead: x, t0, t1, t3
    fe25519_elligator2s_useProvidedScratchBuffers(s, v, t0, t1, t3);
    // dead: t0, t1, t3
    fe25519_mul(eps, s, v); // epsilon = [s] * [c.d^2]
    // dead: t1, t3, v
    // 1/d = s.epsilon.c.d
    fe25519_mul(t3, s, eps); // t3 = [s] * [epsilon]
    // dead: x, t1, v
    fe25519_mul(t1, t3, t2); // t1 = [s.epsilon] * [c.d] = 1/d
    // dead: x, t2, t3, v

    // test that d.1/d = 1
    //fe25519_mul(x, t1, d); // x =? 1
    //fe25519_reduceCompletely(x);

    fe25519_mul(v, t1, &fe25519_minusA); // v = -A/(1 + 2.r^2) = [-A] * [1/d]
    // dead: x, t1, t2, t3

    fe25519_sub (t1, &fe25519_one, eps); // t1 = 1 - [epsilon]
    // dead: x, t0, t2, t3
    fe25519_mul (t2, eps, v); // t2 = [epsilon] * [v]
    // dead: x, t0, t3, v
    fe25519_mul (t3, &fe25519_minusAdiv2, t1); // [-A/2] * [1 - epsilon]
    // dead: x, t0, t1, v
    fe25519_add (x, t2, t3); // x = [epsilon.v] + [-A/2.(1 - epsilon)]
    // dead: t0, t1, t2, t3, v
}

void
fe25519_elligator2_isrt_useFourProvidedScratchBuffers(
	fe25519*       x,
	const fe25519* r,
	// Scratch buffers
	fe25519*       t0,
	fe25519*       t1,
	fe25519*       t2,
	fe25519*       v
	)
{
	fe25519       t3;
	fe25519_elligator2_isrt_useProvidedScratchBuffers(x, r, t0, t1, t2, v, &t3);
}

void
fe25519_elligator2_isrt(
    fe25519*       x,
    const fe25519* r)
{
    // Scratch buffers
    fe25519       t0,t1,t2,t3,v;

    fe25519_elligator2_isrt_useProvidedScratchBuffers (x,r,&t0,&t1,&t2,&t3,&v);
}

void
elligator2_isrt(uint8_t* p, uint8_t* r)
{
	fe25519 Lx, L0;
	
	memcpy(&L0, r, 32);	
	((unsigned char*)&L0)[31]&=0x3F;           //mask two MSB
		fe25519_unpack (&Lx, (unsigned char*)&L0); //254-bits random value
	
	fe25519_elligator2_isrt(&L0,  &Lx);
	fe25519_pack(p, &L0); //output point	
}




