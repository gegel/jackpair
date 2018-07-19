/*
    file 'fe25519_invert.c'

    Van Gegel, 2018

    Creative Commons CC0 1.0 Universal public domain dedication
*/

//#include <inttypes.h>
#include "scalarmult.h"
#include "fe25519_pow2523.h"
/************* Required typedefs ************************/



void
fe25519_elligator2s_useProvidedScratchBuffers(
    fe25519*       r,
    const fe25519* x,
    fe25519*       t0,
    fe25519*       t1,
    fe25519*       t2
);		




		
/************* Inverting function ***************/
		
// Note, that r and x are still allowed to overlap!		

// Note this function requires four scratch buffers
// instead three for old fe25519_invert_useProvidedScratchBuffers() 		
// For call from crypto_scalarmult_curve25519() 
// procedure (see 'scalarmult.c') additional scratch buffer
// is avaliable in ST_curve25519ladderstepWorkingState.s
		
// Usage for DH_SWAP_BY_POINTERS allowed:
// fe25519_elligator2s_invert(state.pZp, state.pZp, state.pXq,state.pZq,&state.x0,&state.s);
// Usage for DH_SWAP_BY_POINTERS denied:
// fe25519_elligator2s_invert(&state.zp, &state.zp, &state.xq, &state.zq, &state.x0, &state.s); 		

//Test on Cortex M3 board (STM32F103CBT6) with Keil uVision compiler,
//optimization -O3 for time:
//-c --cpu Cortex-M3 -D__MICROLIB -g -O3 -Otime --apcs=interwork --split_sections

//crypto_scalarmult_curve25519_base procedure time:
//-for old fe25519_invert_useProvidedScratchBuffers() - 0x3939FD systicks;
//-for new fe25519_elligator2s_invert() - 0x390885 systicks;

//Code size was reduced on 596 bytes
		
void
fe25519_elligator2s_invert(
    fe25519*       r,
    const fe25519* x,
    fe25519*       t0,
    fe25519*       t1,
    fe25519*       t2,
		fe25519*       t3
)
{
  fe25519_elligator2s_useProvidedScratchBuffers(t3, x, t0, t1, t2);
	// r is now x ^ (2^254 - 11)
	fe25519_square(t3, t3); 
	// r is now x ^ (2^255 - 22)
	fe25519_mul(r, t3, x);
	// r is now x ^ (2^255 - 21) 
}
