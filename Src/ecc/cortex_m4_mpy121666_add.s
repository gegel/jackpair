; Implementation of multiplication with 121666 and addition function for cortex M4
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
; Implementation multiply * 121666 with addition.
; 
; implements the interface
;
;void 
;fe25519_mpy121666add_asm(fe25519* out, const fe25519* valueToAdd, const fe25519* valueToMpy);
; in:
;    r0 == ptr to Result word
;    r1 == prt to value to add to result of multiply
;    r2 == prt to value to multiply with 121666
;Note that out and baseValue members are allowed to overlap.

; ######################
; ASM fe25519 MPY with 121666 and add for M4:
; ######################
; START: fe25519 multiplication with 121666 and addition for M4 (MPY121666+ ADD)
; r0 = result ptr, r1,r2 = operand ptr.


	AREA mpyaddcode, CODE
	ENTRY

fe25519_mpy121666add_asm proc
	export fe25519_mpy121666add_asm
		
	push	{r4, r5, r6, r7, lr}
	ldr	r3, label_for_immediate_121666
	ldr	r4, [r1, #28]
	ldr	r6, [r2, #28]
	add.w	r7, r3, r3
	mov	r5, r4
	umaal	r4, r5, r7, r6
	mov.w	r6, #19
	mul.w	r5, r6, r5
	ldr	r7, [r2, #0]
	ldr	r6, [r1, #0]
	umaal	r6, r5, r3, r7
	str	r6, [r0, #0]
	ldr	r7, [r2, #4]
	ldr	r6, [r1, #4]
	umaal	r6, r5, r3, r7
	str	r6, [r0, #4]
	ldr	r7, [r2, #8]
	ldr	r6, [r1, #8]
	umaal	r6, r5, r3, r7
	str	r6, [r0, #8]
	ldr	r7, [r2, #12]
	ldr	r6, [r1, #12]
	umaal	r6, r5, r3, r7
	str	r6, [r0, #12]
	ldr	r7, [r2, #16]
	ldr	r6, [r1, #16]
	umaal	r6, r5, r3, r7
	str	r6, [r0, #16]
	ldr	r7, [r2, #20]
	ldr	r6, [r1, #20]
	umaal	r6, r5, r3, r7
	str	r6, [r0, #20]
	ldr	r7, [r2, #24]
	ldr	r6, [r1, #24]
	umaal	r6, r5, r3, r7
	str	r6, [r0, #24]
	add.w	r4, r5, r4, lsr #1
	str	r4, [r0, #28]
	pop	{r4, r5, r6, r7, pc}
	
	ALIGN
label_for_immediate_121666
	DCD 0x0001db42
	endp
	END