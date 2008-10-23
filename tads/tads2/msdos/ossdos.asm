;
;Notes
;  This module should probably be ifdef'd for RUNTIME
;Modified
;  12/05/90 JEras         - fix ossgmx() for non-EGA type monitors
;  11/25/90 JEras         - add ossgmx()
;  09/14/90 SMcAdams      - fix bug in detecting monochrome monitor
;  03/27/90 JEras         - optimize ossdsp() a little
;  03/23/90 JEras         - add ossmon() and ossdsp() for OS/2 compatibility
;

BIOSINT equ     10h             ; BIOS interrupt number

        EXTRN   _scrbase:DWORD  ; Screen base pointer, from os.c
        EXTRN   _usebios:WORD   ; flag: use BIOS for output, not direct i/o

oss_text segment public 'code'
        assume cs:oss_text


; oss_con_init(void) - initialize console
;
public _oss_con_init
_oss_con_init proc far
    ; we don't need to do anything special - just return
    ret
_oss_con_init endp

; oss_con_uninit(void) - uninitialize console
;
public _oss_con_uninit
_oss_con_uninit proc far
    ; nothing special here
    ret
_oss_con_uninit endp

; oss_set_plain_mode(void) - set plain console mode
;
public _oss_set_plain_mode
_oss_set_plain_mode proc far
    ; nothing special here
    ret
_oss_set_plain_mode endp

;
; ossscu( top_line, left_column, bottom_line, right_column, blank_color )
; -- scroll region down a line
;    line/column numbering starts at zero (0)
;
public  _ossscu
_ossscu proc    far
        push    bp
        mov     bp, sp
        push    si
        push    di

        mov     ch, [bp+6]      ; top line
        mov     cl, [bp+8]      ; left column
        mov     dh, [bp+10]     ; bottom line
        mov     dl, [bp+12]     ; right column
        mov     ax, 0701h       ; function = scroll down 1 line
        mov     bh, [bp+14]     ; color in bh
        int     BIOSINT

        pop     di
        pop     si
        pop     bp
        ret
_ossscu endp


;
; ossscr( top_line, left_column, bottom_line, right_column, blank_color )
; -- scroll region up a line
;    line/column numbering starts at zero (0)
;
public  _ossscr
_ossscr proc    far
        push    bp
        mov     bp, sp
        push    si
        push    di

        mov     ch, [bp+6]      ; top line
        mov     cl, [bp+8]      ; left column
        mov     dh, [bp+10]     ; bottom line
        mov     dl, [bp+12]     ; right column
        mov     ax, 0601h       ; function = scroll up 1 line
        mov     bh, [bp+14]     ; color in bh
        int     BIOSINT

        pop     di
        pop     si
        pop     bp
        ret
_ossscr endp

;
; ossclr( top, left, bottom, right, color )
; -- clear the given region of the screen
;
public  _ossclr
_ossclr proc    far
        push    bp
        mov     bp, sp
        push    si
        push    di

        mov     ch, [bp+6]      ; top line
        mov     cl, [bp+8]      ; left column
        mov     dh, [bp+10]     ; bottom line
        mov     dl, [bp+12]     ; right column
        mov     ax, 0600h       ; function = clear region
        mov     bh, [bp+14]     ; color in bh
        int     BIOSINT

        pop     di
        pop     si
        pop     bp
        ret
_ossclr endp

;
; ossloc( line, column )
; -- locate cursor at given line and column
;    line/column numbers start at zero (0)
;
public  _ossloc
_ossloc proc    far
        push    bp
        mov     bp, sp
        push    di
        push    si

        mov     dh, [bp+6]      ; row
        mov     dl, [bp+8]      ; column
        mov     ah, 2           ; function = set cursor position
        mov     bh, 0           ; page = 0
        int     BIOSINT

        pop     si
        pop     di
        pop     bp
        ret
_ossloc endp

;
; int ossmon()
; -- return 1 if screen is monochrome, 0 otherwise
;    assumes caller wants return value in AX
;
public  _ossmon
_ossmon proc    far
        push    ds
        push    si
        mov     ax, 40h
        mov     ds, ax
        mov     si, 10h         ; byte to check is actually at 40:10
        mov     ax, [si]
        and     ax, 30h
        cmp     ax, 30h
        jz      mono
        xor     ax, ax
        jmp     monret
mono:
        mov     ax, 1
monret:
        pop     si
        pop     ds
        ret
_ossmon endp

;
; ossdsp( int line, int column, int color, char *msg )
; -- display msg with color at coordinates (line,column)
;    this routine could probably be optimized some, but it seems to work okay
;    assumes large model
;
public  _ossdsp
_ossdsp proc far
        push    bp
        mov     bp, sp
        push    ds
        push    es
        push    si

	cmp	[_usebios], 0
	je	use_direct_io

	; use BIOS to output the characters
	lds	si, [bp+12]	; get the string pointer

bios_loop:
	; locate the cursor
	push	word ptr [bp+8]
	push	word ptr [bp+6]
	call	_ossloc
	add	sp, 4

	inc	word ptr [bp+8]

	; write the next character
	mov	bl, [bp+10]	; get the color
	mov	ah, 9		; function = display character
	mov	bh, 0		; page = 0
	mov	cx, 1		; count = 1
	lodsb			; get next character (mov al, es:[si++])
	cmp	al, 0		; done?
	je	dspret		; yes, quit

	int	BIOSINT
	jmp	bios_loop

use_direct_io:
        lds     si, [_scrbase]
        mov     ax, [bp+6]
        mov     cx, 80
        mul     cx
        add     ax, [bp+8]
        shl     ax, 1
        add     si, ax

        les     bx, [bp+12]
        mov     ah, [bp+10]
dsploop:
        mov     al, es:[bx]
        cmp     al, 0
        jz      dspret
        mov     [si], ax
        add     si, 2
        inc     bx
        jmp     dsploop
dspret:
        pop     si
        pop     es
        pop     ds
        pop     bp
        ret
_ossdsp endp

;
; ossgmx( int *max_line, int *max_column )
; -- return max screen size in *max_line and *max_column
;    assumes large data model
;
public  _ossgmx
_ossgmx proc far
        push    bp
        mov     bp, sp

        push    es
        push    di

        mov     dl, 18h                 ; 24 rows by default (bias of 0)
        xor     bh, bh
        mov     ax, 1130h               ; get font info
        push    bp
        int     BIOSINT
        pop     bp
        les     di, [bp+6]              ; load max_line address
        mov     byte ptr es:[di], dl    ; DL=rows on screen, save it
        mov     byte ptr es:[di+1], 0

        mov     ah, 0fh                 ; get current video mode
        int     BIOSINT
        dec     ah                      ; AH=columns on screen - 1
        les     di, [bp+10]             ; get max_column address
        mov     byte ptr es:[di], ah    ; save columns
        mov     byte ptr es:[di+1], 0

        pop     di
        pop     es

        pop     bp
        ret
_ossgmx endp

oss_text ends
        end
