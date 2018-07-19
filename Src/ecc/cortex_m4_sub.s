; Implementation of substraction function for cortex M4
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
; Implementation of substracting.
; 
; implements the interface
;
;void
;fe25519_sub_asm(fe25519* out, const fe25519* baseValue, const fe25519* valueToSubstract);
;	
; in:
;    r0 == ptr to Result word
;    r1 == prt to base value 
;    r2 == prt to value to substract

; ######################
; ASM fe25519 SUB for M4:
; ######################
; START: fe25519 substract for M4
; r0 = result ptr, r1,r2 = operand ptr.


	AREA subcode, CODE
	ENTRY

fe25519_sub_asm proc
	export fe25519_sub_asm
		
	push	{r4, r5, r6, lr}
	movs	r3, #0
	ldr	r4, [r1, #28]
	ldr	r5, [r2, #28]
	subs	r4, r4, r5
	sbcs	r5, r5
	adds	r6, r4, r4
	adcs	r5, r5
	orr.w	r4, r4, #2147483648	; 0x80000000
	str	r4, [r0, #28]
	sub.w	r4, r5, #1
	mvn.w	r5, #18
	mul.w	r4, r4, r5
	ldr	r5, [r2, #0]
	ldr	r6, [r1, #0]
	umaal	r5, r4, r3, r3
	subs	r6, r6, r5
	str	r6, [r0, #0]
	ldr	r5, [r2, #4]
	ldr	r6, [r1, #4]
	umaal	r5, r4, r3, r3
	sbcs	r6, r5
	str	r6, [r0, #4]
	ldr	r5, [r2, #8]
	ldr	r6, [r1, #8]
	umaal	r5, r4, r3, r3
	sbcs	r6, r5
	str	r6, [r0, #8]
	ldr	r5, [r2, #12]
	ldr	r6, [r1, #12]
	umaal	r5, r4, r3, r3
	sbcs	r6, r5
	str	r6, [r0, #12]
	ldr	r5, [r2, #16]
	ldr	r6, [r1, #16]
	umaal	r5, r4, r3, r3
	sbcs	r6, r5
	str	r6, [r0, #16]
	ldr	r5, [r2, #20]
	ldr	r6, [r1, #20]
	umaal	r5, r4, r3, r3
	sbcs	r6, r5
	str	r6, [r0, #20]
	ldr	r5, [r2, #24]
	ldr	r6, [r1, #24]
	umaal	r5, r4, r3, r3
	sbcs	r6, r5
	str	r6, [r0, #24]
	ldr	r6, [r0, #28]
	sbcs	r6, r4
	str	r6, [r0, #28]
	pop	{r4, r5, r6, pc}

	endp
	END