; Implementation of addition function for cortex M4
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
; Implementation of addition.
; 
; implements the interface
;
;void
;fe25519_add_asm(fe25519* out, const fe25519* in1, const fe25519* in2);
;	
; in:
;    r0 == ptr to Result word
;    r1, r2 == prts to values to add

; ######################
; ASM fe25519 ADD for M4:
; ######################
; START: fe25519 addition for M4
; r0 = result ptr, r1,r2 = operand ptr.


	AREA addcode, CODE
	ENTRY

fe25519_add_asm proc
	export fe25519_add_asm
		
	push	{r4, r5, r6, r7, lr}
	movs	r3, #1
	ldr	r5, [r1, #28]
	ldr	r4, [r2, #28]
	mov	r6, r4
	umaal	r5, r6, r5, r3
	umlal	r5, r6, r4, r3
	mov.w	r4, #19
	mul.w	r4, r6, r4
	ldr	r6, [r1, #0]
	ldr	r7, [r2, #0]
	umaal	r6, r7, r3, r4
	str	r6, [r0, #0]
	ldr	r4, [r1, #4]
	ldr	r6, [r2, #4]
	umaal	r6, r7, r3, r4
	str	r6, [r0, #4]
	ldr	r4, [r1, #8]
	ldr	r6, [r2, #8]
	umaal	r6, r7, r3, r4
	str	r6, [r0, #8]
	ldr	r4, [r1, #12]
	ldr	r6, [r2, #12]
	umaal	r6, r7, r3, r4
	str	r6, [r0, #12]
	ldr	r4, [r1, #16]
	ldr	r6, [r2, #16]
	umaal	r6, r7, r3, r4
	str	r6, [r0, #16]
	ldr	r4, [r1, #20]
	ldr	r6, [r2, #20]
	umaal	r6, r7, r3, r4
	str	r6, [r0, #20]
	ldr	r4, [r1, #24]
	ldr	r6, [r2, #24]
	umaal	r6, r7, r3, r4
	str	r6, [r0, #24]
	add.w	r7, r7, r5, lsr #1
	str	r7, [r0, #28]
	pop	{r4, r5, r6, r7, pc}

	endp
	END