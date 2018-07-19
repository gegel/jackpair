; Implementation of squaring function for cortex M4
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
; Implementation of fe25519 square.
; uses packed non-redundant representation.
; 
; 
; implements the interface
;
; void
; fe25519_square_asm (fe25519 *pResult, const fe25519 *pVal1);
;	
; in:
;    r0 == ptr to Result word
;    r1 == prt to value to square
;
; Reduces modulo such that the result fits in 256 bits, i.e. not necessarily fully
; reduced!

; ######################
; ASM fe25519 Square and reduce for M4:
; ######################
; START: fe25519 squaring for M4 (MPY + partial reduce)
; r0 = result ptr, r1 = operand ptr.

	AREA squarecode, CODE
	ENTRY

fe25519_square_asm proc
	export fe25519_square_asm

	push {r0, r4, r5, r6, r7, r8, r9, sl, fp, ip, lr}
	sub	sp, #20
	ldr	r0, [r1, #0]
	ldr	r2, [r1, #4]
	ldr	r3, [r1, #8]
	ldr	r4, [r1, #12]
	ldr	r5, [r1, #16]
	ldr	r6, [r1, #20]
	umull	r7, r8, r0, r2
	umull	r9, sl, r0, r0
	umaal	r7, sl, r0, r2
	umaal	r8, sl, r2, r2
	str.w	r9, [sp]
	str	r7, [sp, #4]
	umull	r7, r9, r0, r4
	umull	fp, ip, r0, r3
	adds.w	fp, fp, fp
	eor.w	lr, lr, lr
	umaal	r8, fp, r8, lr
	umaal	sl, fp, sl, lr
	str.w	r8, [sp, #8]
	umaal	r7, ip, r2, r3
	adcs	r7, r7
	umaal	r7, sl, r7, lr
	str	r7, [sp, #12]
	add	sl, fp
	ldr	r7, [r1, #24]
	ldr.w	r8, [r1, #28]
	umull	r1, fp, r0, r6
	umaal	r9, ip, r0, r5
	umaal	r1, ip, r2, r5
	umaal	fp, ip, r0, r7
	umlal	r9, lr, r2, r4
	umaal	r1, lr, r3, r4
	umaal	fp, lr, r2, r6
	umaal	ip, lr, r0, r8
	adcs.w	r9, r9, r9
	adcs	r1, r1
	eor.w	r0, r0, r0
	umaal	r9, sl, r3, r3
	str.w	r9, [sp, #16]
	umaal	r1, sl, r1, r0
	umlal	fp, r0, r3, r5
	umaal	r0, ip, r2, r7
	umaal	ip, lr, r2, r8
	adcs.w	fp, fp, fp
	umaal	sl, fp, r4, r4
	eor.w	r2, r2, r2
	umlal	r0, r2, r3, r6
	umaal	r2, ip, r3, r7
	umaal	ip, lr, r3, r8
	eor.w	r3, r3, r3
	umlal	r0, r3, r4, r5
	umaal	r2, r3, r4, r6
	umaal	r3, ip, r4, r7
	umaal	ip, lr, r4, r8
	adcs	r0, r0
	eor.w	r9, r9, r9
	umaal	r0, fp, r0, r9
	adcs	r2, r2
	umlal	r3, r9, r5, r6
	umaal	r9, ip, r5, r7
	umaal	ip, lr, r5, r8
	eor.w	r4, r4, r4
	umlal	ip, r4, r6, r7
	umaal	r4, lr, r6, r8
	umaal	r2, fp, r5, r5
	eor.w	r5, r5, r5
	adcs	r3, r3
	umaal	r3, fp, r3, r5
	umlal	lr, r5, r7, r8
	adcs.w	r9, r9, r9
	adcs.w	ip, ip, ip
	adcs	r4, r4
	adcs.w	lr, lr, lr
	adcs	r5, r5
	umaal	r9, fp, r6, r6
	eor.w	r6, r6, r6
	umaal	fp, ip, fp, r6
	umaal	r4, ip, r7, r7
	umaal	ip, lr, ip, r6
	umaal	r5, lr, r8, r8
	adcs.w	lr, lr, r6
	mov.w	r7, #38	; 0x26
	umlal	r0, r6, lr, r7
	mov.w	r8, #19
	mov.w	lr, r0, lsr #31
	mul.w	r8, lr, r8
	mov.w	r0, r0, lsl #1
	mov.w	r0, r0, lsr #1
	ldr.w	lr, [sp]
	umaal	r8, lr, r2, r7
	eor.w	r2, r2, r2
	umlal	r8, r2, r6, r7
	add	r2, lr
	ldr	r6, [sp, #20]
	ldr.w	lr, [sp, #4]
	str.w	r8, [r6]
	umaal	r2, lr, r3, r7
	str	r2, [r6, #4]
	ldr	r2, [sp, #8]
	ldr	r3, [sp, #12]
	ldr.w	r8, [sp, #16]
	umaal	r2, lr, r9, r7
	str	r2, [r6, #8]
	umaal	r3, lr, fp, r7
	str	r3, [r6, #12]
	umaal	r8, lr, r4, r7
	str.w	r8, [r6, #16]
	umaal	r1, lr, ip, r7
	str	r1, [r6, #20]
	umaal	sl, lr, r5, r7
	str.w	sl, [r6, #24]
	add	r0, lr
	str	r0, [r6, #28]
	add	sp, #24
	pop {r4, r5, r6, r7, r8, r9, sl, fp, ip, pc}
	
	endp
	END
