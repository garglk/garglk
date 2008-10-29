; Copyright (c) 1991 by Michael J. Roberts.  All Rights Reserved.
;
; This is the TADS User Exit wrapper.  Use this to generate a user
; exit function.  Calls your user exit's main function main(),
; which receives one argument, a uxdef (see TADSEXIT.H).
;

_TEXT	segment para public 'CODE'
	assume  cs:_TEXT
	extrn	_main:near

	org	0
$entry:
	push	bp		; save BP
	mov	bp, sp		; establish stack frame
	push	ds		; save other needed registers
	push	es

	push	[bp+8]		; push the uxdef pointer (high part)
	push	[bp+6]		;                        (low part)
	call	_main		; call user's function
	add	sp, 4		; remove arguments from stack

	pop	es		; restore all registers
	pop	ds
	pop	bp
	retf
_TEXT	ends

; dummy out this symbol for MSC - it insists on its presence
public  __acrtused
        __acrtused = 0

;public  __aNlshl
;        __aNlshl = 0
	end $entry
