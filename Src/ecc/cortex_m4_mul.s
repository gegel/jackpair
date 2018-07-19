; Implementation of multiplication function for cortex M4
;
; The code is derived from the simulation code in the tests/asmM0.cc file.
; 
; B. Haase, Endress + Hauser Conducta GmbH & Ko. KG
;
; public domain
;
; Code is now tested on the target and on the PC.

;
; Keil uVision assembler format
;

; ****************************************************
; Implementation of 256x256 bit bit multiply with partial reduction.
; uses packed non-redundant representation.
; 
; 
; implements the interface
;
; void
; fe25519_mul_asm (fe25519 *pResult, const fe25519 *pVal1, const fe25519 *pVal2);
;	
; in:
;    r0 == ptr to Result word
;    r1, r2 == prts to values to multiply

; ######################
; ASM fe25519 MPY and reduce for M4:
; ######################
; START: fe25519 multiplication for M4 (MPY + partial reduce)
; r0 = result ptr, r1,r2 = operand ptr.
 
	AREA mulcode, CODE
	ENTRY

fe25519_mul_asm proc
	export fe25519_mul_asm
		
	push {r0, r1, r2, r4, r5, r6, r7, r8, r9, sl, fp, ip, lr}
	sub	sp, #32
	ldr	r0, [r2, #0]
	ldr	r3, [r2, #4]
	ldr	r4, [r2, #8]
	ldr	r5, [r2, #12]
	ldr	r6, [r2, #16]
	ldr	r7, [r1, #0]
	umull	r8, r9, r7, r3
	umull	sl, fp, r7, r0
	str.w	sl, [sp]
	eor.w	sl, sl, sl
	umull	ip, lr, sl, sl
	umlal	r9, sl, r7, r4
	umlal	sl, ip, r7, r5
	umlal	ip, lr, r7, r6
	ldr	r7, [r1, #4]
	umaal	r8, fp, r7, r0
	umaal	r9, fp, r7, r3
	umaal	sl, fp, r7, r4
	umaal	fp, ip, r7, r5
	str.w	r8, [sp, #4]
	eor.w	r8, r8, r8
	umaal	ip, lr, r7, r6
	ldr	r7, [r1, #8]
	umlal	r9, r8, r7, r0
	umaal	r8, sl, r7, r3
	umaal	sl, fp, r7, r4
	umaal	fp, ip, r7, r5
	str.w	r9, [sp, #8]
	eor.w	r9, r9, r9
	umaal	ip, lr, r7, r6
	ldr	r7, [r1, #12]
	umlal	r8, r9, r7, r0
	umaal	r9, sl, r7, r3
	umaal	sl, fp, r7, r4
	umaal	fp, ip, r7, r5
	str.w	r8, [sp, #12]
	eor.w	r8, r8, r8
	umaal	ip, lr, r7, r6
	ldr	r7, [r1, #16]
	umlal	r9, r8, r7, r0
	umaal	r8, sl, r7, r3
	umaal	sl, fp, r7, r4
	umaal	fp, ip, r7, r5
	str.w	r9, [sp, #16]
	eor.w	r9, r9, r9
	umaal	ip, lr, r7, r6
	ldr	r2, [r1, #20]
	umlal	r8, r9, r2, r0
	umaal	r9, sl, r2, r3
	umaal	sl, fp, r2, r4
	ldr	r2, [r1, #24]
	eor.w	r7, r7, r7
	umlal	r9, r7, r2, r0
	umaal	r7, sl, r2, r3
	umaal	sl, fp, r2, r4
	ldr	r2, [r1, #28]
	umaal	sl, ip, r2, r3
	umaal	fp, ip, r2, r4
	ldr	r0, [sp, #40]	; 0x28
	ldr	r3, [r0, #20]
	ldr	r4, [r0, #24]
	ldr	r5, [r1, #0]
	eor.w	r6, r6, r6
	umlal	r8, r6, r5, r3
	umaal	r6, r9, r5, r4
	str.w	r8, [sp, #20]
	ldr.w	r8, [r0, #28]
	umaal	r7, r9, r5, r8
	ldr	r5, [r1, #4]
	eor.w	r2, r2, r2
	umlal	r6, r2, r5, r3
	str	r6, [sp, #24]
	umaal	r2, r7, r5, r4
	umaal	r7, r9, r5, r8
	ldr	r5, [r1, #8]
	eor.w	r6, r6, r6
	umlal	r2, r6, r5, r3
	umaal	r6, r7, r5, r4
	umaal	r7, r9, r5, r8
	ldr	r5, [r1, #12]
	umaal	r6, sl, r5, r3
	umaal	r7, sl, r5, r4
	umaal	r9, sl, r5, r8
	ldr	r5, [r1, #16]
	umaal	r7, fp, r5, r3
	umaal	r9, fp, r5, r4
	umaal	sl, fp, r5, r8
	ldr	r5, [r1, #28]
	ldr	r3, [r0, #0]
	eor.w	r4, r4, r4
	umlal	r2, r4, r5, r3
	str	r2, [sp, #28]
	ldr	r2, [r1, #24]
	ldr	r3, [r1, #20]
	ldr	r1, [r0, #12]
	umaal	r4, r6, r3, r1
	umaal	r6, r7, r2, r1
	umaal	r7, r9, r5, r1
	ldr	r1, [r0, #16]
	umaal	r6, lr, r3, r1
	umaal	r7, ip, r2, r1
	umaal	r9, sl, r5, r1
	ldr	r1, [r0, #20]
	umaal	r7, lr, r3, r1
	umaal	r9, ip, r2, r1
	umaal	sl, fp, r5, r1
	ldr	r1, [r0, #24]
	umaal	r9, lr, r3, r1
	umaal	sl, ip, r2, r1
	umaal	fp, ip, r5, r1
	umaal	sl, lr, r3, r8
	umaal	fp, lr, r2, r8
	umaal	ip, lr, r5, r8
	ldr	r0, [sp, #28]
	ldr	r1, [sp, #0]
	ldr	r2, [sp, #4]
	mov.w	r3, #38	; 0x26
	eor.w	r5, r5, r5
	umlal	r0, r5, lr, r3
	mov.w	r8, #19
	mov.w	lr, r0, lsr #31
	mul.w	r8, lr, r8
	mov.w	r0, r0, lsl #1
	mov.w	r0, r0, lsr #1
	umaal	r1, r8, r4, r3
	eor.w	r4, r4, r4
	umlal	r1, r4, r5, r3
	add	r2, r4
	umaal	r2, r8, r6, r3
	ldr	r4, [sp, #8]
	ldr	r5, [sp, #12]
	ldr	r6, [sp, #16]
	ldr.w	lr, [sp, #20]
	umaal	r4, r8, r7, r3
	umaal	r5, r8, r9, r3
	umaal	r6, r8, sl, r3
	umaal	r8, lr, fp, r3
	ldr	r7, [sp, #24]
	ldr.w	r9, [sp, #32]
	umaal	r7, lr, ip, r3
	add	r0, lr
	str.w	r0, [r9, #28]
	str.w	r7, [r9, #24]
	str.w	r8, [r9, #20]
	str.w	r6, [r9, #16]
	str.w	r5, [r9, #12]
	str.w	r4, [r9, #8]
	str.w	r2, [r9, #4]
	str.w	r1, [r9]
	add	sp, #44	; 0x2c
	pop	{r4, r5, r6, r7, r8, r9, sl, fp, ip, pc}
	
	endp
	END
