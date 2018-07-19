/* dspfns.h
 *
 * Copyright 2001,2014 ARM Limited. All rights reserved.
 *
 * RCS $Revision$
 * Checkin $Date$
 * Revising $Author$
 */

/* ----------------------------------------------------------------------
 * This header file provides a set of DSP-type primitive
 * operations, such as 16-bit and 32-bit saturating arithmetic. The
 * operations it provides are similar to the ones used by the ITU
 * for publishing specifications of DSP algorithms.
 */

#ifndef ARMDSP_DSPFNS_H
#define ARMDSP_DSPFNS_H
#define __ARMCLIB_VERSION 5060009

#include "constant.h"

#ifdef __cplusplus
#define __STDC_LIMIT_MACROS 1
#define __STDC_FORMAT_MACROS 1
#define __STDC_CONSTANT_MACROS 1
#endif /* __cplusplus */
#include <stdint.h>
#include <assert.h>

#include <math.h>

#ifndef MAX_16
#define MAX_16 INT16_MAX
#define MIN_16 INT16_MIN
#define MAX_32 INT32_MAX
#define MIN_32 INT32_MIN
#endif /* MAX_16 etc. */

#if 1
#ifndef __TARGET_FEATURE_DSPMUL
//#error ETSI intrinsics not currently emulated on this platform
#endif /* __TARGET_FEATURE_DSPMUL */

#if defined(__thumb) && (__TARGET_ARCH_THUMB < 4)
#error ETSI intrinsics not available on Thumb-1
#endif /* Thumb but not Thumb-2 */
#endif

#ifdef __cplusplus
#define __ARM_INTRINSIC __forceinline
#elif defined __GNUC__ || defined _USE_STATIC_INLINE
#define __ARM_INTRINSIC static __forceinline
#elif (defined(__STDC_VERSION__) && 199901L <= __STDC_VERSION__)
#define __ARM_INTRINSIC __forceinline
#else
#define __ARM_INTRINSIC __forceinline
#endif

/* Define this to 1 if you do not need add() etc. to set the saturation flag */
#ifndef __ARM_DSP_IGNORE_OVERFLOW
#define __ARM_DSP_IGNORE_OVERFLOW 1
#endif

/* Define this to 1 if you believe all shift counts are in the range [-255,255] */
#ifndef __ARM_DSP_SMALL_SHIFTS
#define __ARM_DSP_SMALL_SHIFTS 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

	
	//--------------------------------------------------------------------
__ARM_INTRINSIC int64_t melpe_L40_shl(int64_t acc, int16_t var1);
__ARM_INTRINSIC int64_t melpe_L40_shr(int64_t acc, int16_t var1);
__ARM_INTRINSIC int64_t melpe_L40_satup(int64_t acc);
__ARM_INTRINSIC int64_t melpe_L40_satdown(int64_t acc);
//--------------------------------------------------------------------
	
	
	
#ifdef __TARGET_FEATURE_DSPMUL

#pragma recognize_itu_functions /* enable vectorization of ITU functions */

#if !defined(__ARM_BIG_ENDIAN) && !defined(__BIG_ENDIAN)

typedef union {
  struct {
    int _dnm:27;
    int Q:1;
    int V:1;
    int C:1;
    int Z:1;
    int N:1;
  } b;
  unsigned int word;
} _ARM_PSR;

#else /* defined(__ARM_BIG_ENDIAN) || defined(__BIG_ENDIAN) */

typedef union {
  struct {
    int N:1;
    int Z:1;
    int C:1;
    int V:1;
    int Q:1;
    int _dnm:27;
  } b;
  unsigned int word;
} _ARM_PSR;

#endif /* defined(__ARM_BIG_ENDIAN) || defined(__BIG_ENDIAN) */

register _ARM_PSR _apsr_for_q __asm("apsr");
#define Overflow _apsr_for_q.b.Q

#else

__ARM_INTRINSIC int *_arm_global_overflow(void) {
    static int v;
    return &v;
}

#define Overflow (*_arm_global_overflow())




__ARM_INTRINSIC int32_t __qadd(int32_t x, int32_t y)
{
    int32_t r;
#if __TARGET_ARCH_ARM > 0
    int ov = 0;
    __asm {
        adds r, x, y
        movvs ov, #1
    }
    if (ov) {
#if !__ARM_DSP_IGNORE_OVERFLOW
        Overflow = 1;
#endif
        r = y < 0 ? INT32_MIN : INT32_MAX;
    }
#else
    r = x + y;
    if (y > 0 && r < x) {
#if !__ARM_DSP_IGNORE_OVERFLOW
        Overflow = 1;
#endif
        return INT32_MAX;
    } else if (y < 0 && r > x) { 
#if !__ARM_DSP_IGNORE_OVERFLOW
        Overflow = 1;
#endif
        return INT32_MIN;
    }
#endif
    return r;
}

 


__ARM_INTRINSIC int32_t __qsub(int32_t x, int32_t y)
{
    int32_t r;
#if __TARGET_ARCH_ARM > 0
    int ov = 0;
    __asm {
        subs r, x, y
        movvs ov, #1
    }
    if (ov) {
#if !__ARM_DSP_IGNORE_OVERFLOW
        Overflow = 1;
#endif
        r = y >= 0 ? INT32_MIN : INT32_MAX;
    }
#else
    r = x - y;
    if (y > 0 && r > x) {
#if !__ARM_DSP_IGNORE_OVERFLOW
        Overflow = 1;
#endif
        return INT32_MIN;
    } else if (y < 0 && r < x) {
#if !__ARM_DSP_IGNORE_OVERFLOW
        Overflow = 1;
#endif
        return INT32_MAX;
    }
#endif
    return r;
}

__ARM_INTRINSIC int32_t __qdbl(int32_t x)
{
    return __qadd(x, x);
}

#endif

__ARM_INTRINSIC int *_arm_global_carry(void) {
    
	  extern int c_carry; 
    return &c_carry;
}

#define Carry (*_arm_global_carry())

/*
 * Convert a 32-bit signed integer into a 16-bit signed integer by
 * saturation.
 */
__ARM_INTRINSIC int16_t melpe_saturate(int32_t x)
{
#if (defined(__thumb) && (__TARGET_ARCH_THUMB >= 4)) || (__TARGET_ARCH_ARM >= 6)
    return (int16_t)__ssat(x, 16);
#else
    /* ARM v5E has no SSAT instruction */      
    if (x > INT16_MAX || x < INT16_MIN)
        x = __qdbl(INT32_MAX - ((x) >> 31)) >> 16;   /* Saturate and set Overflow */
    return (int16_t) x;
#endif
}

/*
 * Add two 16-bit signed integers with saturation.
 */
__ARM_INTRINSIC int16_t melpe_add(int16_t x, int16_t y)
{
#if __ARM_DSP_IGNORE_OVERFLOW && ((defined(__thumb) && (__TARGET_ARCH_THUMB >= 4)) || (__TARGET_ARCH_ARM >= 6))
    return (int16_t)__qadd16(x, y);
#else
    return (int16_t)(__qadd(x<<16, y<<16) >> 16);
#endif
}

/*
 * Subtract one 16-bit signed integer from another with saturation.
 */
__ARM_INTRINSIC int16_t melpe_sub(int16_t x, int16_t y)
{
#if __ARM_DSP_IGNORE_OVERFLOW && ((defined(__thumb) && (__TARGET_ARCH_THUMB >= 4)) || (__TARGET_ARCH_ARM >= 6))
    return (int16_t)__qsub16(x, y);
#else
    return (int16_t)(__qsub(x<<16, y<<16) >> 16);
#endif
}

/*
 * Absolute value of a 16-bit signed integer. Saturating, so
 * abs(-0x8000) becomes +0x7FFF.
 */
__ARM_INTRINSIC int16_t melpe_abs_s(int16_t x)
{
    if (x >= 0)
        return x;
#if (defined(__thumb3) && (__TARGET_ARCH_THUMB >= 4)) || (__TARGET_ARCH_ARM >= 6)
    return (int16_t)__qsub16(0, x);
#else
    else if (x == INT16_MIN)
        return INT16_MAX;
    else
        return (int16_t) -x;
#endif
}

/*
 * Shift a 16-bit signed integer left (or right, if the shift count
 * is negative). Saturate on overflow.
 */
__ARM_INTRINSIC int16_t melpe_shl(int16_t x, int16_t shift)
{
    if (shift <= 0 || x == 0) {
#if !__ARM_DSP_SMALL_SHIFTS
        if (shift < -63) shift = -63;
#endif /* __ARM_DSP_SMALL_SHIFTS */
        return (int16_t) (x >> (-shift));
    }
    if (shift > 15)
        shift = 16;
    return melpe_saturate(x << shift);
}

/*
 * Shift a 16-bit signed integer right (or left, if the shift count
 * is negative). Saturate on overflow.
 */
__ARM_INTRINSIC int16_t melpe_shr(int16_t x, int16_t shift)
{
    if (shift >= 0 || x == 0) {
#if !__ARM_DSP_SMALL_SHIFTS
        if (shift > 63) shift = 63;
#endif /* __ARM_DSP_SMALL_SHIFTS */
        return (int16_t) (x >> shift);
    }
    if (shift < -15)
        shift = -16;
    return melpe_saturate(x << (-shift));
}

/*
 * Multiply two 16-bit signed integers, shift the result right by
 * 15 and saturate it. (Saturation is only necessary if both inputs
 * were -0x8000, in which case the result "should" be 0x8000 and is
 * saturated to 0x7FFF.)
 */
__ARM_INTRINSIC int16_t melpe_mult(int16_t x, int16_t y)
{
    return (int16_t)(__qdbl(x*y) >> 16);
}

/*
 * Multiply two 16-bit signed integers to give a 32-bit signed
 * integer. Shift left by one, and saturate the result. (Saturation
 * is only necessary if both inputs were -0x8000, in which case the
 * result "should" be 0x40000000 << 1 = +0x80000000, and is
 * saturated to +0x7FFFFFFF.)
 */
__ARM_INTRINSIC int32_t melpe_L_mult(int16_t x, int16_t y)
{
    return __qdbl(x*y);
}

/*
 * Negate a 16-bit signed integer, with saturation. (Saturation is
 * only necessary when the input is -0x8000.)
 */
__ARM_INTRINSIC int16_t melpe_negate(int16_t x)
{
#if (defined(__thumb3) && (__TARGET_ARCH_THUMB >= 4)) || (__TARGET_ARCH_ARM >= 6)
    return (int16_t)__qsub16(0, x);
#else
    if (x == INT16_MIN)
        return INT16_MAX;
    return (int16_t) -x;
#endif
}

/*
 * Return the top 16 bits of a 32-bit signed integer.
 */
__ARM_INTRINSIC int16_t melpe_extract_h(int32_t x)
{
    return (int16_t) (x >> 16);
}

/*
 * Return the bottom 16 bits of a 32-bit signed integer, with no
 * saturation, just coerced into a two's complement 16 bit
 * representation.
 */
__ARM_INTRINSIC int16_t melpe_extract_l(int32_t x)
{
    return (int16_t) x;
}

/*
 * Divide a 32-bit signed integer by 2^16, rounding to the nearest
 * integer (round up on a tie). Equivalent to adding 0x8000 with
 * saturation, then shifting right by 16.
 */
__ARM_INTRINSIC int16_t melpe_r_ound(int32_t x)
{
    return melpe_extract_h(__qadd(x, 0x8000));
}

/*
 * Multiply two 16-bit signed integers together to give a 32-bit
 * signed integer, shift left by one with saturation, and add to
 * another 32-bit integer with saturation.
 * 
 * Note the intermediate saturation operation in the definition:
 * 
 *    L_mac(-1, -0x8000, -0x8000)
 * 
 * will give 0x7FFFFFFE and not 0x7FFFFFFF:
 *    the unshifted product is:   0x40000000
 *    shift left with saturation: 0x7FFFFFFF
 *    add to -1 with saturation:  0x7FFFFFFE
 */
__ARM_INTRINSIC int32_t melpe_L_mac(int32_t accumulator, int16_t x, int16_t y)
{
    return __qadd(accumulator, __qdbl(x*y));
}

/*
 * Multiply two 16-bit signed integers together to give a 32-bit
 * signed integer, shift left by one with saturation, and subtract
 * from another 32-bit integer with saturation.
 * 
 * Note the intermediate saturation operation in the definition:
 * 
 *    L_msu(1, -0x8000, -0x8000)
 * 
 * will give 0x80000002 and not 0x80000001:
 *    the unshifted product is:         0x40000000
 *    shift left with saturation:       0x7FFFFFFF
 *    subtract from 1 with saturation:  0x80000002
 */
__ARM_INTRINSIC int32_t melpe_L_msu(int32_t accumulator, int16_t x, int16_t y)
{
    return __qsub(accumulator, __qdbl(x*y));
}

/*
 * Add two 32-bit signed integers with saturation.
 */
__ARM_INTRINSIC int32_t melpe_L_add(int32_t x, int32_t y)
{
    return __qadd(x, y);
}

/*
 * Subtract one 32-bit signed integer from another with saturation.
 */
__ARM_INTRINSIC int32_t melpe_L_sub(int32_t x, int32_t y)
{
    return __qsub(x, y);
}

/*
 * Add together the Carry variable and two 32-bit signed integers,
 * without saturation.
 * Note: the reference implementation has INT32_MIN + -1 + (Carry=1)
 * set the cumulative overflow flag.  This does not match intuition,
 * or the natural behavior of ARM's ADCS instruction.
 */
__ARM_INTRINSIC int32_t melpe_L_add_c(int32_t x, int32_t y)
{
    int32_t result;
#if __TARGET_ARCH_ARM > 0
    int32_t flags;
    __asm {
        movs flags, Carry, lsr #1
        adcs result, x, y;
        mrs flags, CPSR;
    }
#if !__ARM_DSP_IGNORE_OVERFLOW
    if (flags & 0x10000000) Overflow = 1;  /* V -> Q */
#endif
    Carry = (flags & 0x20000000) != 0;
#else
    /* Inline assembler not available */
    result = x + y + Carry;
    Carry = (uint32_t)((x & y) | ((x | y) & ~result)) >> 31;
#if !__ARM_DSP_IGNORE_OVERFLOW
    if (((result ^ x) & (result ^ y) & 0x80000000) != 0) Overflow = 1;
#endif
#endif
    return result;
}

/*
 * Subtract one 32-bit signed integer, together with the logical
 * negation of the Carry variable, from another 32-bit signed integer,
 * without saturation.
 * N.b. the computation matches that of the ETSI reference function
 * (in basicop2.c).  The comment above the ETSI reference function says
 * that L_sub_c(a,b) = a-b-C, but that does not match their code.
 */
__ARM_INTRINSIC int32_t melpe_L_sub_c(int32_t x, int32_t y)
{
    int32_t result;
#if __TARGET_ARCH_ARM > 0
    int32_t flags;
    __asm {
        movs flags, Carry, lsr #1
        sbcs result, x, y;
        mrs flags, CPSR;
    }
#if !__ARM_DSP_IGNORE_OVERFLOW
    if (flags & 0x10000000) Overflow = 1;  /* V -> Q */
#endif
    Carry = (flags & 0x20000000) != 0;
#else
    /* Inline assembler not available */
    result = x + ~y + Carry;
    Carry = ((uint32_t)((x & ~y) | ((x | ~y) & ~result)) >> 31);
#if !__ARM_DSP_IGNORE_OVERFLOW
    if (((x ^ y) & (result ^ y) & 0x80000000) != 0) Overflow = 1;
#endif
#endif
    return result;
}

/*
 * Multiply two 16-bit signed integers together to give a 32-bit
 * signed integer, shift left by one _with_ saturation, and add
 * with carry to another 32-bit integer _without_ saturation.
 */
__ARM_INTRINSIC int32_t melpe_L_macNs(int32_t accumulator, int16_t x, int16_t y)
{
    return melpe_L_add_c(accumulator, melpe_L_mult(x, y));
}

/*
 * Multiply two 16-bit signed integers together to give a 32-bit
 * signed integer, shift left by one _with_ saturation, and
 * subtract with carry from another 32-bit integer _without_
 * saturation.
 */
__ARM_INTRINSIC int32_t melpe_L_msuNs(int32_t accumulator, int16_t x, int16_t y)
{
    return melpe_L_sub_c(accumulator, melpe_L_mult(x, y));
}

/*
 * Negate a 32-bit signed integer, with saturation. (Saturation is
 * only necessary when the input is -0x80000000.)
 */
__ARM_INTRINSIC int32_t melpe_L_negate(int32_t x)
{
    return __qsub(0, x);
}

/*
 * Multiply two 16-bit signed integers, shift the result right by
 * 15 with rounding, and saturate it. (Saturation is only necessary
 * if both inputs were -0x8000, in which case the result "should"
 * be 0x8000 and is saturated to 0x7FFF.)
 */
__ARM_INTRINSIC int16_t melpe_mult_r(int16_t x, int16_t y)
{
    return (int16_t)(__qdbl(x*y + 0x4000) >> 16);
}

/*
 * Return the number of bits of left shift needed to arrange for a
 * 16-bit signed integer to have value >= 0x4000 or <= -0x4001.
 * 
 * Returns 0 if x is zero (following C reference implementation).
 */
__ARM_INTRINSIC int16_t melpe_norm_s(int16_t x)
{
    return __clz(x ^ ((int32_t)x << 17)) & 15;
}

/*
 * Return the number of bits of left shift needed to arrange for a
 * 32-bit signed integer to have value >= 0x40000000 (if +ve)
 * or <= -0x40000001 (if -ve).
 * 
 * Returns 0 if x is zero (following C reference implementation).
 */
__ARM_INTRINSIC int16_t melpe_norm_l(int32_t x)
{
    return __clz(x ^ (x << 1)) & 31;
}

/*
 * Shift a 32-bit signed integer left (or right, if the shift count
 * is negative). Saturate on overflow.
 */
__ARM_INTRINSIC int32_t melpe_L_shl(int32_t x, int16_t shift)
{
    if (shift <= 0) {
#if !__ARM_DSP_SMALL_SHIFTS
        if (shift < -63) shift = -63;
#endif /* __ARM_DSP_SMALL_SHIFTS */
        return x >> (-shift);
    }
    if (shift <= melpe_norm_l(x) || x == 0)
        return x << shift;
    return __qdbl((x < 0) ? INT32_MIN : INT32_MAX);
}

/*
 * Shift a 32-bit signed integer right (or left, if the shift count
 * is negative). Saturate on overflow.
 */
__ARM_INTRINSIC int32_t melpe_L_shr(int32_t x, int16_t shift)
{
    if (shift >= 0) {
#if !__ARM_DSP_SMALL_SHIFTS
        if (shift > 63) shift = 63;
#endif /* __ARM_DSP_SMALL_SHIFTS */
        return x >> shift;
    }
    if ((-shift) <= melpe_norm_l(x) || x == 0)
        return x << (-shift);
    return __qdbl((x < 0) ? INT32_MIN : INT32_MAX);
}

/*
 * Shift a 16-bit signed integer right, with rounding. Shift left
 * with saturation if the shift count is negative.
 */
__ARM_INTRINSIC int16_t melpe_shift_r(int16_t x, int16_t shift)
{
    if (shift == 0 || x == 0)
        return (int16_t)x;
		shift=-shift;
    if (shift > 0) {
#if !__ARM_DSP_SMALL_SHIFTS
        if (shift > 32) shift = 32;
#endif /* __ARM_DSP_SMALL_SHIFTS */
        return (int16_t) (((x >> (shift-1)) + 1) >> 1);
    }
    if (shift < -15)
        shift = -16;
    return melpe_saturate(x << (-shift));
}

/*
 * Multiply two 16-bit signed integers together to give a 32-bit
 * signed integer, shift left by one with saturation, and add to
 * another 32-bit integer with saturation (like L_mac). Then shift
 * the result right by 15 bits with rounding (like round).
 */
__ARM_INTRINSIC int16_t melpe_mac_r(int32_t accumulator, int16_t x, int16_t y)
{
    return melpe_r_ound(melpe_L_mac(accumulator, x, y));
}

/*
 * Multiply two 16-bit signed integers together to give a 32-bit
 * signed integer, shift left by one with saturation, and subtract
 * from another 32-bit integer with saturation (like L_msu). Then
 * shift the result right by 15 bits with rounding (like round).
 */
__ARM_INTRINSIC int16_t melpe_msu_r(int32_t accumulator, int16_t x, int16_t y)
{
    return melpe_r_ound(melpe_L_msu(accumulator, x, y));
}

/*
 * Shift a 16-bit signed integer left by 16 bits to generate a
 * 32-bit signed integer. The bottom 16 bits are zeroed.
 */
__ARM_INTRINSIC int32_t melpe_L_deposit_h(int16_t x)
{
    return ((int32_t)x) << 16;
}

/*
 * Sign-extend a 16-bit signed integer by 16 bits to generate a
 * 32-bit signed integer.
 */
__ARM_INTRINSIC int32_t melpe_L_deposit_l(int16_t x)
{
    return (int32_t)x;
}

/*
 * Shift a 32-bit signed integer right, with rounding. Shift left
 * with saturation if the shift count is negative.
 */
__ARM_INTRINSIC int32_t melpe_L_shr_r(int32_t x, int16_t shift)
{
    if (shift == 0 || x == 0)
        return x;
    if (shift > 0) {
#if !__ARM_DSP_SMALL_SHIFTS
        int32_t x2 = (shift > 32) ? 0 : x >> (shift-1);
#else
        int32_t x2 = x >> (shift-1);
#endif /* __ARM_DSP_SMALL_SHIFTS */
        return (x2 >> 1) + (x2 & 1);
    }
    if (-shift <= melpe_norm_l(x) || x == 0)
        return x << (-shift);
    return __qdbl((x < 0) ? INT32_MIN : INT32_MAX);
}

/*
 * Absolute value of a 32-bit signed integer. Saturating, so
 * abs(-0x80000000) becomes +0x7FFFFFFF.
 */
__ARM_INTRINSIC int32_t melpe_L_abs(int32_t x)
{
    if (x >= 0)
        return x;
    else
        return __qsub(0, x);
}

/*
 * Return a saturated value appropriate to the most recent carry-
 * affecting operation (L_add_c, L_macNs, L_sub_c, L_msuNs).
 * 
 * In other words: return the argument if the Q flag is clear.
 * Otherwise, return -0x80000000 or +0x7FFFFFFF depending on
 * whether the Carry flag is set or clear (respectively).
 */
__ARM_INTRINSIC int32_t melpe_L_sat(int32_t x)
{
    if (Overflow) {
        Overflow = 0;
        x = (int32_t)((uint32_t)INT32_MAX + Carry);
        Carry = 0;
    }
    return x;
}

/*
 * Divide one 16-bit signed integer by another, and produce a
 * 15-bit fixed point fractional result (by multiplying the true
 * mathematical result by 0x8000). The divisor (denominator) is
 * assumed to be non-zero and also assumed to be greater or equal
 * to the dividend (numerator). Hence the (unscaled) result is
 * necessarily within the range [0,1].
 * 
 * Both operands are assumed to be positive.
 * 
 * After division, the result is saturated to fit into a 16-bit
 * signed integer. (The only way this can happen is if the operands
 * are equal, so that the result would be 1, i.e. +0x8000 in 15-bit
 * fixed point.)
 */
/*
__ARM_INTRINSIC int16_t melpe_divide_s(int16_t x, int16_t y)
{
    int32_t quot;
    assert(y > 0);
    assert(x >= 0);
    assert(x <= y);
    quot = 0x8000 * x;
    quot /= y;
    if (quot > INT16_MAX)
        return INT16_MAX;
    else
        return (int16_t)quot;
}

*/
__ARM_INTRINSIC  int16_t melpe_divide_s(int16_t var1, int16_t var2)
{
	int32_t L_div;
	int16_t swOut;

	if (var1 < 0 || var2 < 0 || var1 > var2) {
		/* undefined output for invalid input into divide_s() */
		return ((int16_t) 0);
	}

	if (var1 == var2)
		return ((int16_t) 0x7fff);

	L_div = ((0x00008000L * (int32_t) var1) / (int32_t) var2);
	swOut = melpe_saturate(L_div);
	return (swOut);
}



__ARM_INTRINSIC int64_t melpe_L40_satup(int64_t acc)
{
	if(acc > MAX_40) acc=MAX_40;
	return (acc);
}

__ARM_INTRINSIC int64_t melpe_L40_satdown(int64_t acc)
{
	if(acc < MIN_40) acc=MIN_40;
	return (acc);
}








//###################################################################################

/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L40_add                                                 |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   40 bits addition of 40 bits accumulator with 32 bits variable (L_var1)  |
 |   with overflow control and saturation; the result is set at MAX40        |
 |   when overflow occurs or at MIN40 when underflow occurs.                 |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |                                                                           |
 |    L_var1   32 bit long signed integer (int32_t) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var1 <= 0x7fff ffff.                 |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |___________________________________________________________________________|
*/

static inline int64_t melpe_L40_add(int64_t acc, int32_t L_var1)
{
	

	acc = acc + (int64_t) L_var1;

	//if (acc > MAX_40) {
	//	acc = MAX_40;
		/* Overflow = 1; */
	//}
	//if (acc < MIN_40) {
	//	acc = MIN_40;
		/* Overflow = 1; */
	//}
	return (acc);
}

/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L40_sub                                                 |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   40 bits subtraction of 40 bits accumulator with 32 bits variable        |
 |   (L_var1) with overflow control and saturation; the result is set at     |
 |   MAX40 when overflow occurs or at MIN40 when underflow occurs.           |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |                                                                           |
 |    L_var1   32 bit long signed integer (int32_t) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var1 <= 0x7fff ffff.                 |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |___________________________________________________________________________|
*/

static inline int64_t melpe_L40_sub(int64_t acc, int32_t L_var1)
{
	

	acc = acc - (int64_t) L_var1;

	//if (acc > MAX_40) {
	//	acc = MAX_40;
		/* Overflow = 1; */
	//}
	//if (acc < MIN_40) {
	//	acc = MIN_40;
		/* Overflow = 1; */
	//}
	return (acc);
}

/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L40_mac                                                 |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Multiply var1 by var2 and shift the result left by 1. Add the result    |
 |   to 40 bits accumulator with overflow control and saturation; the result |
 |   is set at MAX40 when overflow occurs or at MIN40 when underflow occurs. |
 |                                                                           |
 |   Complexity weight : 1                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |                                                                           |
 |    var1     16 bit short signed integer (int16_t) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |    var2     16 bit short signed integer (int16_t) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |___________________________________________________________________________|
*/

static inline int64_t melpe_L40_mac(int64_t acc, int16_t var1, int16_t var2)
{
	
	acc+=((int32_t)var1*var2*2);
	
	//if (acc > MAX_40) {
	//	acc = MAX_40;
		
	//}
	//if (acc < MIN_40) {
	//	acc = MIN_40;
		
	//}
	return acc;
	
	
/*
	acc = acc + ((int64_t) var1 * (int64_t) var2 * 2);

	if (acc > MAX_40) {
		acc = MAX_40;
		
	}
	if (acc < MIN_40) {
		acc = MIN_40;
		
	}

	return (acc);
	*/	

}

/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L40_msu                                                 |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Multiply var1 by var2 and shift the result left by 1. Subtract the      |
 |   result to 40 bits accumulator with overflow control and saturation;     |
 |   the result is set at MAX40 when overflow occurs or at MIN40 when        |
 |   underflow occurs.                                                       |
 |                                                                           |
 |   Complexity weight : 1                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |                                                                           |
 |    var1     16 bit short signed integer (int16_t) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |    var2     16 bit short signed integer (int16_t) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |___________________________________________________________________________|
*/

static inline int64_t melpe_L40_msu(int64_t acc, int16_t var1, int16_t var2)
{
	
	acc-=((int32_t)var1*var2*2);
	
	//if (acc > MAX_40) {
	//	acc = MAX_40;
		
	//}
	//if (acc < MIN_40) {
	//	acc = MIN_40;
		
	//}
	return acc;
	
	
	/*

	acc = acc - ((int64_t) var1 * (int64_t) var2 * 2);

	if (acc > MAX_40) {
		acc = MAX_40;
		
	}
	if (acc < MIN_40) {
		acc = MIN_40;
		
	}
	 
	
	return (acc);
	*/
}

/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L40_shl                                                 |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Arithmetically shift the 40 bits accumulator left var1 positions. Zero  |
 |   fill the var1 LSB of the result. If var1 is negative, arithmetically    |
 |   shift 40 bits accumulator right by -var1 with sign extension. Saturate  |
 |   the result in case of underflows or overflows.                          |              |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |                                                                           |
 |    var1     16 bit short signed integer (int16_t) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |___________________________________________________________________________|
*/

static inline int64_t melpe_L40_shl(int64_t acc, int16_t var1)
{



	if (var1 < 0) {
        acc = melpe_L40_shr(acc, (int16_t) (-var1));
	} else {
		for (; var1 > 0; var1--) {
			//acc = acc * 2;
			acc<<=1;
			if (acc > MAX_40) {
				acc = MAX_40;
				break;
			} else if (acc < MIN_40) {
				acc = MIN_40;
				break;
			}
		}
	}
	return (acc);
}

/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L40_shr                                                 |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Arithmetically shift the 40 bits accumulator right var1 positions with  |
 |   sign extension. If var1 is negative, arithmetically shift 40 bits       |
 |   accumulator left by -var1 and zero fill the var1 LSB of the result.     |
 |   Saturate the result in case of underflows or overflows.                 |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |                                                                           |
 |    var1     16 bit short signed integer (int16_t) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |___________________________________________________________________________|
*/

static inline int64_t melpe_L40_shr(int64_t acc, int16_t var1)
{

	

	if (var1 < 0) {
        acc = melpe_L40_shl(acc, (int16_t) - var1);
	} else {
		for (; var1 > 0; var1--) {
			//acc = floorf(acc * 0.5f);
			acc>>=1;
			if ((acc == 0) || (acc == -1))
				break;
		}
	}
	return (acc);
}

/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L40_negate                                              |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Negate the 40 bits accumulator with saturation; saturate to MAX40 in    |
 |   the case where input is MIN40.                                          |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |___________________________________________________________________________|
*/

static inline int64_t melpe_L40_negate(int64_t acc)
{
	

	acc = -acc;

	//if (acc > MAX_40) {
	//	acc = MAX_40;
	//}
	return (acc);
}

/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : norm32                                                  |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Produces the number of left shift needed to normalize the 40 bits       |
 |   accumulator in 32 bits format (for positive value on the interval with  |
 |   minimum of 1073741824 and maximum of 2147483647, and for negative value |
 |   on the interval with minimum of -2147483648 and maximum of -1073741824);|
 |   in order to normalize the result, the following operation must be done: |
 |                   norm32_acc = L40_shl(acc, norm32(acc)).                 |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    var1     16 bit short signed integer (int16_t) whose value falls in the |
 |             range : -8 <= var_out <= 31.                                  |
 |___________________________________________________________________________|
*/

static inline int16_t melpe_norm32(int64_t acc)
{
	int16_t var1;

	
	var1 = 0;

	if (acc > 0) {
		while (acc > (int64_t) MAX_32) {
			//acc = floorf(acc * 0.5f);
			acc>>=1;
			var1--;
		}
		while (acc <= (0.5f * (int64_t) MAX_32)) {
			//acc = acc * 2;
			acc<<=1;
			var1++;
		}
	} else if (acc < 0) {
		while (acc < (int64_t) MIN_32) {
			//acc = floorf(acc * 0.5f);
			acc>>=1;
			var1--;
		}
		while (acc >= (0.5f * (int64_t) MIN_32)) {
			//acc = acc * 2;
			acc<<=1;
			var1++;
		}
	}
	return (var1);
}

/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L_sat32                                                 |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |    Saturate the 40 bits accumulator to the range of a 32 bits word.       |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    acc      40 bits accumulator (int64_t) whose value falls in the         |
 |             range : MIN40 <= acc <= MAX40.                                |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (int32_t) whose value falls in the  |
 |             range : 0x8000 0000 <= var_out <= 0x7fff ffff.                |
 |___________________________________________________________________________|
*/

static inline int32_t melpe_L_sat32(int64_t acc)
{
	int32_t L_var_out;

	

	if (acc > (int64_t) MAX_32) {
		acc = (int64_t) MAX_32;
	}
	if (acc < (int64_t) MIN_32) {
		acc = (int64_t) MIN_32;
	}
	L_var_out = (int32_t) acc;
	return (L_var_out);
}






#ifdef __cplusplus
}
#endif

#endif /* ARMDSP_DSPFNS_H */

