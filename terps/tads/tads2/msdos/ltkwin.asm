	.386p
	ifndef	??version
?debug	macro
	endm
	endif
	?debug	S "ltkwin.c"
	?debug	T "ltkwin.c"
_TEXT	segment byte public use32 'CODE'
_TEXT	ends
_DATA	segment dword public use32 'DATA'
_DATA	ends
_BSS	segment dword public use32 'BSS'
_BSS	ends
$$BSYMS	segment byte public use32 'DEBSYM'
$$BSYMS	ends
$$BTYPES	segment byte public use32 'DEBTYP'
$$BTYPES	ends
$$BNAMES	segment byte public use32 'DEBNAM'
$$BNAMES	ends
$$BROWSE	segment byte public use32 'DEBSYM'
$$BROWSE	ends
$$BROWFILE	segment byte public use32 'DEBSYM'
$$BROWFILE	ends
DGROUP	group	_DATA,_BSS
	assume	cs:_TEXT,ds:DGROUP
_TEXT	segment byte public use32 'CODE'
c@	label	byte
	assume	cs:_TEXT
_ltkini	proc	near
   ;	
   ;	void ltkini(heapsiz)
   ;	
	?debug	L 69
@11@37:
	push	ebp
	mov	ebp,esp
	push	edi
	push	esi
	mov	edi,dword ptr [ebp+8]
   ;	
   ;	  ub2 heapsiz;                                    /* size of the local heap */
   ;	{
   ;	  HANDLE  memhdl;                                          /* memory handle */
   ;	  HANDLE  syshdl;                                     /* sys context handle */
   ;	  ub2     segd;	                                  /* the segment descriptor */
   ;	  void   *heap;                                       /* ptr to heap memory */
   ;	
   ;	  /* allocate and lock heap segment */
   ;	  if (!(ltkhdl = GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT, heapsiz)) ||
   ;	
	?debug	L 78
@12@28:
	push	edi
	push	64
	call	near ptr GlobalAlloc
@12@77:
	test	eax,eax
	mov	dword ptr ltkhdl,eax
@12@119:
	je	short @14
	push	dword ptr ltkhdl
	call	near ptr GlobalLock
	mov	esi,eax
	test	esi,esi
	jne	short @15
@14:
   ;	
   ;	      !(heap = GlobalLock(ltkhdl)))
   ;	  {
   ;	    /* no luck allocating memory */
   ;	    return;
   ;	
	?debug	L 82
	jmp	near ptr @18
@15:
   ;	
   ;	  }
   ;	
   ;	  /*
   ;	   * get the actual size allocated.  subtract 16 for Windows overhead
   ;	   * (see doc for LocalInit)
   ;	   */
   ;	  heapsiz = GlobalSize(ltkhdl) - 16;
   ;	
	?debug	L 89
	push	dword ptr ltkhdl
	call	near ptr GlobalSize
	lea	edi,dword ptr [eax-16]
   ;	
   ;	
   ;	  /*
   ;	   * set DS to new segment 
   ;	   */
   ;	  ltkseg = HIWORD(heap);                                 /* get the segment */
   ;	
	?debug	L 94
	mov	eax,esi
	shr	eax,16
	and	ax,-1
	movzx	eax,ax
@15@134:
	mov	dword ptr ltkseg,eax
   ;	
   ;	  INLINE
   ;	  {
   ;	    push ds
   ;	
	?debug	L 97
@15@161:
	push	 ds
   ;	
   ;	    mov  ax, ltkseg
   ;	
	?debug	L 98
	mov	  ax, dword ptr ltkseg
   ;	
   ;	    mov  ds, ax
   ;	
	?debug	L 99
	mov	  ds, ax
   ;	
   ;	  }
   ;	
   ;	  /* initialize the heap segment */
   ;	  if (!LocalInit(ltkseg, 0, heapsiz))
   ;	
	?debug	L 103
	push	edi
	push	0
	push	dword ptr ltkseg
	call	near ptr _LocalInit
	add	esp,12
	test	eax,eax
	jne	short @17
   ;	
   ;	  {
   ;	    /* restore DS */
   ;	    INLINE
   ;	    {
   ;	      pop ds
   ;	
	?debug	L 108
	pop	 ds
   ;	
   ;	    }
   ;	
   ;	    /* unlock and free the global memory */
   ;	    GlobalUnlock(ltkhdl);
   ;	
	?debug	L 112
	push	dword ptr ltkhdl
	call	near ptr GlobalUnlock
   ;	
   ;	    GlobalFree(ltkhdl);
   ;	
	?debug	L 113
	push	dword ptr ltkhdl
	call	near ptr GlobalFree
   ;	
   ;	
   ;	    /* can't use heap */
   ;	    ltkhdl = 0;
   ;	
	?debug	L 116
	mov	dword ptr ltkhdl,0
   ;	
   ;	    ltkseg = 0;
   ;	
	?debug	L 117
@16@139:
	mov	dword ptr ltkseg,0
   ;	
   ;	
   ;	    /* no more we can do */
   ;	    return;
   ;	
	?debug	L 120
@16@164:
	jmp	short @18
@17:
   ;	
   ;	  }
   ;	
   ;	  /*
   ;	   * restore the data segment 
   ;	   */
   ;	  INLINE
   ;	  {
   ;	    pop ds
   ;	
	?debug	L 128
	pop	 ds
	?debug	L 130
@17@10:
@18:
@18@0:
	pop	esi
	pop	edi
	pop	ebp
	ret
_ltkini	endp
	assume	cs:_TEXT
_ltkfre	proc	near
   ;	
   ;	void ltkfre()
   ;	
	?debug	L 137
@20@37:
	push	ebp
	mov	ebp,esp
   ;	
   ;	{
   ;	  /* unlock and free the heap segment */
   ;	  GlobalUnlock(ltkhdl);
   ;	
	?debug	L 140
@21@0:
	push	dword ptr ltkhdl
	call	near ptr GlobalUnlock
   ;	
   ;	  GlobalFree(ltkhdl);
   ;	
	?debug	L 141
	push	dword ptr ltkhdl
	call	near ptr GlobalFree
   ;	
   ;	
   ;	  /* reset statics */
   ;	  ltkhdl = 0;
   ;	
	?debug	L 144
	mov	dword ptr ltkhdl,0
   ;	
   ;	  ltkseg = 0;
   ;	
	?debug	L 145
@21@129:
	mov	dword ptr ltkseg,0
	?debug	L 146
@21@154:
@22:
@22@0:
	pop	ebp
	ret
_ltkfre	endp
	assume	cs:_TEXT
_ltk_suballoc	proc	near
   ;	
   ;	void *ltk_suballoc(siz)
   ;	
	?debug	L 153
@24@43:
	push	ebp
	mov	ebp,esp
	push	edi
	push	esi
   ;	
   ;	  ub2  siz;                                             /* size to allocate */
   ;	{
   ;	  HANDLE    memhdl;                                        /* memory handle */
   ;	  ltk_mblk *mblk;                                           /* memory block */
   ;	
   ;	  /* assert that we have a heap segment */
   ;	  if (!ltkseg || !ltkhdl) return((void *)0);
   ;	
	?debug	L 160
@25@0:
	cmp	dword ptr ltkseg,0
	je	short @27
	cmp	dword ptr ltkhdl,0
	jne	short @28
@27:
	xor	eax,eax
	?debug	L 195
	jmp	near ptr @35
@28:
   ;	
   ;	
   ;	  /* verify that we can allocate the requested size */
   ;	  if ((ub4)siz + (ub4)sizeof(ltk_mblk) > 0x10000L) return((void *)0);
   ;	
	?debug	L 163
	mov	eax,dword ptr [ebp+8]
	add	eax,5
	cmp	eax,65536
	jbe	short @30
	xor	eax,eax
	?debug	L 195
	jmp	short @35
@30:
   ;	
   ;	
   ;	  /* use the heap's data segment */
   ;	  INLINE
   ;	  {
   ;	    push ds
   ;	
	?debug	L 168
	push	 ds
   ;	
   ;	    mov  ax, ltkseg
   ;	
	?debug	L 169
	mov	  ax, dword ptr ltkseg
   ;	
   ;	    mov  ds, ax
   ;	
	?debug	L 170
	mov	  ds, ax
   ;	
   ;	  }
   ;	
   ;	  /* allocate and lock a block of the requested size */
   ;	  if (!(memhdl = LocalAlloc(LMEM_FIXED|LMEM_ZEROINIT,
   ;	
	?debug	L 174
	mov	eax,dword ptr [ebp+8]
	add	eax,5
	push	eax
	push	64
	call	near ptr LocalAlloc
	mov	edi,eax
	test	edi,edi
	je	short @32
	push	edi
	call	near ptr LocalLock
	mov	esi,eax
	test	esi,esi
	jne	short @33
@32:
   ;	
   ;				    sizeof(ltk_mblk)+siz)) ||
   ;	      !(mblk = (ltk_mblk *)LocalLock(memhdl)))
   ;	  {
   ;	    /* fix up data segment */
   ;	    INLINE
   ;	    {
   ;	      pop ds
   ;	
	?debug	L 181
	pop	 ds
   ;	
   ;	    }
   ;	    return((void *)0);
   ;	
	?debug	L 183
	xor	eax,eax
	?debug	L 195
	jmp	short @35
@33:
   ;	
   ;	  }
   ;	
   ;	  /* return to our own data segment */
   ;	  INLINE
   ;	  {
   ;	      pop ds
   ;	
	?debug	L 189
	pop	 ds
   ;	
   ;	  }
   ;	
   ;	  /* stash the handle and we're on our way */
   ;	  mblk->mblkhdl = memhdl;
   ;	
	?debug	L 193
	mov	dword ptr [esi],edi
   ;	
   ;	  return(mblk->mblkbuf);
   ;	
	?debug	L 194
	lea	eax,dword ptr [esi+4]
	?debug	L 195
@35:
@35@0:
	pop	esi
	pop	edi
	pop	ebp
	ret
_ltk_suballoc	endp
	assume	cs:_TEXT
_ltk_sigsuballoc	proc	near
   ;	
   ;	void *ltk_sigsuballoc(errcx, siz)
   ;	
	?debug	L 202
@38@46:
	push	ebp
	mov	ebp,esp
	push	edi
   ;	
   ;	  errcxdef *errcx;                          /* error context to signal with */
   ;	  ub2       siz;                                        /* size to allocate */
   ;	{
   ;	  void *ptr;                                     /* ptr to allocated memory */
   ;	
   ;	  /* allocate the memory */
   ;	  if (!(ptr = ltk_suballoc(siz)))
   ;	
	?debug	L 209
@39@0:
	push	dword ptr [ebp+12]
	call	near ptr _ltk_suballoc
	add	esp,4
	mov	edi,eax
	test	edi,edi
	jne	short @41
   ;	
   ;	  {
   ;	    /* signal an error */
   ;	    errsigf(errcx, "LTK", 0);
   ;	
	?debug	L 212
	mov	eax,offset DGROUP:s@
	push	0
	push	eax
	push	dword ptr [ebp+8]
	call	near ptr _errsigf
	add	esp,12
@41:
   ;	
   ;	  }
   ;	
   ;	  /* return the memory */
   ;	  return(ptr);
   ;	
	?debug	L 216
	mov	eax,edi
	?debug	L 217
@43:
@43@0:
	pop	edi
	pop	ebp
	ret
_ltk_sigsuballoc	endp
	assume	cs:_TEXT
_ltk_subfree	proc	near
   ;	
   ;	void ltk_subfree(ptr)
   ;	
	?debug	L 224
@51@42:
	push	ebp
	mov	ebp,esp
	push	edi
   ;	
   ;	  void *ptr;                                               /* block to free */
   ;	{
   ;	  ltk_mblk *mblk;		                        /* the memory block */
   ;	  
   ;	  /* assert that we have a heap segment */
   ;	  if (!ltkseg || !ltkhdl) return;
   ;	
	?debug	L 230
@52@0:
	cmp	dword ptr ltkseg,0
	je	short @54
	cmp	dword ptr ltkhdl,0
	jne	short @55
@54:
	jmp	short @56
@55:
   ;	
   ;	
   ;	  /* get our handle */
   ;	  mblk = (ltk_mblk *)((ub1 *)ptr - offsetof(ltk_mblk, mblkbuf));
   ;	
	?debug	L 233
	mov	edi,dword ptr [ebp+8]
	add	edi,-4
   ;	
   ;	
   ;	  /* use the heap's seg as DS */
   ;	  INLINE
   ;	  {
   ;	    push ds
   ;	
	?debug	L 238
	push	 ds
   ;	
   ;	    mov  ax, ltkseg
   ;	
	?debug	L 239
	mov	  ax, dword ptr ltkseg
   ;	
   ;	    mov  ds, ax
   ;	
	?debug	L 240
	mov	  ds, ax
   ;	
   ;	  }
   ;	
   ;	  /* unlock and free the memory */
   ;	  LocalUnlock(mblk->mblkhdl);
   ;	
	?debug	L 244
	push	dword ptr [edi]
	call	near ptr LocalUnlock
   ;	
   ;	  LocalFree(mblk->mblkhdl);
   ;	
	?debug	L 245
	push	dword ptr [edi]
	call	near ptr LocalFree
   ;	
   ;	
   ;	  /* finally, return to our regular DS */
   ;	  INLINE
   ;	  {
   ;	    pop ds
   ;	
	?debug	L 250
	pop	 ds
	?debug	L 252
@55@206:
@56:
@56@0:
	pop	edi
	pop	ebp
	ret
_ltk_subfree	endp
	assume	cs:_TEXT
_ltk_alloc	proc	near
   ;	
   ;	void *ltk_alloc(siz)
   ;	
	?debug	L 261
@58@40:
	push	ebp
	mov	ebp,esp
	push	edi
	push	esi
   ;	
   ;	  size_t  siz;	                               /* size of block to allocate */
   ;	{
   ;	  HANDLE    memhdl;                                        /* memory handle */
   ;	  ltk_mblk *mblk;                                           /* memory block */
   ;	  
   ;	  /* verify that the we can allocate the requested size */
   ;	  if ((ub4)siz+(ub4)sizeof(ltk_mblk) >= 0x10000) return((void *)0);
   ;	
	?debug	L 268
@59@0:
	mov	eax,dword ptr [ebp+8]
	add	eax,5
	cmp	eax,65536
	jb	short @61
	xor	eax,eax
	?debug	L 280
	jmp	short @67
@61:
   ;	
   ;	  
   ;	  /* allocate a block of the requested size */
   ;	  memhdl = GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT, sizeof(ltk_mblk)+siz);
   ;	
	?debug	L 271
	mov	eax,dword ptr [ebp+8]
	add	eax,5
	push	eax
	push	64
	call	near ptr GlobalAlloc
	mov	edi,eax
   ;	
   ;	  if (!memhdl) return((void *)0);
   ;	
	?debug	L 272
	test	edi,edi
	jne	short @63
	xor	eax,eax
	?debug	L 280
	jmp	short @67
@63:
   ;	
   ;	  
   ;	  /* lock the memory, stash the handle, and we're outta here */
   ;	  mblk = (ltk_mblk *)GlobalLock(memhdl);
   ;	
	?debug	L 275
	push	edi
	call	near ptr GlobalLock
	mov	esi,eax
   ;	
   ;	  if (!mblk) return((void *)0);
   ;	
	?debug	L 276
	test	esi,esi
	jne	short @65
	xor	eax,eax
	?debug	L 280
	jmp	short @67
@65:
   ;	
   ;	  
   ;	  mblk->mblkhdl = memhdl;
   ;	
	?debug	L 278
	mov	dword ptr [esi],edi
   ;	
   ;	  return(mblk->mblkbuf);
   ;	
	?debug	L 279
	lea	eax,dword ptr [esi+4]
	?debug	L 280
@67:
@67@0:
	pop	esi
	pop	edi
	pop	ebp
	ret
_ltk_alloc	endp
	assume	cs:_TEXT
_ltk_sigalloc	proc	near
   ;	
   ;	void *ltk_sigalloc(errcx, siz)
   ;	
	?debug	L 287
@70@43:
	push	ebp
	mov	ebp,esp
	push	edi
   ;	
   ;	  errcxdef *errcx;                                         /* error context */
   ;	  size_t    siz;                                        /* size to allocate */
   ;	{
   ;	  void *ptr;                                 /* pointer to allocated memory */
   ;	
   ;	  if (!(ptr = ltk_alloc(siz)))
   ;	
	?debug	L 293
@71@0:
	push	dword ptr [ebp+12]
	call	near ptr _ltk_alloc
	add	esp,4
	mov	edi,eax
	test	edi,edi
	jne	short @73
   ;	
   ;	  {
   ;	    /* signal error */
   ;	    errsigf(errcx, "LTK", 0);
   ;	
	?debug	L 296
	mov	eax,offset DGROUP:s@+4
	push	0
	push	eax
	push	dword ptr [ebp+8]
	call	near ptr _errsigf
	add	esp,12
@73:
   ;	
   ;	  }
   ;	
   ;	  /* return a ptr to the allocated memory */
   ;	  return(ptr);
   ;	
	?debug	L 300
	mov	eax,edi
	?debug	L 301
@75:
@75@0:
	pop	edi
	pop	ebp
	ret
_ltk_sigalloc	endp
	assume	cs:_TEXT
_ltk_free	proc	near
   ;	
   ;	void ltk_free(mem)
   ;	
	?debug	L 310
@77@39:
	push	ebp
	mov	ebp,esp
	push	edi
   ;	
   ;	  void *mem;                                              /* memory to free */
   ;	{
   ;	  ltk_mblk *mblk;                                       /* the memory block */
   ;	  
   ;	  /* get our handle */
   ;	  mblk = (ltk_mblk *)((ub1 *)mem - offsetof(ltk_mblk, mblkbuf));
   ;	
	?debug	L 316
@78@0:
	mov	edi,dword ptr [ebp+8]
	add	edi,-4
   ;	
   ;	  
   ;	  /* unlock and free the memory, and we're outta here */
   ;	  GlobalUnlock(mblk->mblkhdl);
   ;	
	?debug	L 319
	push	dword ptr [edi]
	call	near ptr GlobalUnlock
   ;	
   ;	  GlobalFree(mblk->mblkhdl);
   ;	
	?debug	L 320
	push	dword ptr [edi]
	call	near ptr GlobalFree
   ;	
   ;	  return;
   ;	
	?debug	L 321
@80:
	?debug	L 322
@80@0:
	pop	edi
	pop	ebp
	ret
_ltk_free	endp
	assume	cs:_TEXT
_ltk_errlog	proc	near
   ;	
   ;	void ltk_errlog(ctx, fac, errno, argc, argv)
   ;	
	?debug	L 331
@83@41:
	enter	256,0
	push	edi
	mov	edi,dword ptr [ebp+16]
   ;	
   ;	  void    *ctx;                                     /* error logger context */
   ;	  char    *fac;                                            /* facility code */
   ;	  int      errno;                                           /* error number */
   ;	  int      argc;                                          /* argument count */
   ;	  erradef *argv;                                               /* arguments */
   ;	{
   ;	  char buf[128];                                  /* formatted error buffer */
   ;	  char msg[128];                                          /* message buffer */
   ;	
   ;	  /* $$$ filter out error #504 $$$ */
   ;	  if (errno == 504) return;
   ;	
	?debug	L 342
@84@29:
	cmp	edi,504
	je	short @87
@86:
   ;	
   ;	
   ;	  /* get the error message into msg */
   ;	  errmsg(ctx, msg, sizeof(msg), errno);
   ;	
	?debug	L 345
	lea	eax,byte ptr [ebp-128]
	push	edi
	push	128
	push	eax
	push	dword ptr [ebp+8]
	call	near ptr _errmsg
	add	esp,16
   ;	
   ;	
   ;	  /* format the error message */
   ;	  errfmt(buf, (int)sizeof(buf), msg, argc, argv);
   ;	
	?debug	L 348
	lea	eax,byte ptr [ebp-256]
	lea	edx,byte ptr [ebp-128]
	push	dword ptr [ebp+24]
	push	dword ptr [ebp+20]
	push	edx
	push	128
	push	eax
	call	near ptr _errfmt
	add	esp,20
   ;	
   ;	
   ;	  /* display a dialog box containing the error message */
   ;	  ltk_dlg("Error", buf);
   ;	
	?debug	L 351
	mov	eax,offset DGROUP:s@+8
	lea	edx,byte ptr [ebp-256]
	push	edx
	push	eax
	call	near ptr _ltk_dlg
	add	esp,8
	?debug	L 352
@86@421:
@87:
@87@0:
	pop	edi
	leave
	ret
_ltk_errlog	endp
	assume	cs:_TEXT
_ltk_dlg	proc	near
   ;	
   ;	void ltk_dlg(char *title, char *msg, ...)
   ;	
	?debug	L 359
@90@38:
	enter	240,0
	push	ebx
   ;	
   ;	#ifdef NEVER
   ;	  char *title;                                         /* message box title */
   ;	  char *msg;                                          /* error message text */
   ;	#endif
   ;	{
   ;	  va_list  argp;                                             /* printf args */
   ;	  char     inbuf[80];                                       /* input buffer */
   ;	  char     outbuf[160];                    /* allow inbuf to double in size */
   ;	
   ;	  /* clip the input message, if necessary */
   ;	  strncpy(inbuf, msg, sizeof(inbuf));
   ;	
	?debug	L 370
@91@0:
	lea	eax,byte ptr [ebp-80]
	push	80
	push	dword ptr [ebp+12]
	push	eax
	call	near ptr _strncpy
	add	esp,12
   ;	
   ;	  inbuf[sizeof(inbuf)-1] = '\0';
   ;	
	?debug	L 371
	mov	byte ptr [ebp-1],0
   ;	
   ;	
   ;	  /* get the printf args, build the message, and display it */
   ;	  va_start(argp, msg);
   ;	
	?debug	L 374
	lea	eax,byte ptr [ebp+16]
   ;	
   ;	  vsprintf(outbuf, inbuf, argp);
   ;	
	?debug	L 375
@91@166:
	lea	edx,byte ptr [ebp-240]
	lea	ebx,byte ptr [ebp-80]
	push	eax
	push	ebx
	push	edx
@91@256:
	call	near ptr _vsprintf
	add	esp,12
   ;	
   ;	
   ;	  /* display the message */
   ;	  MessageBox((HWND)NULL, outbuf, title,
   ;	
	?debug	L 378
	lea	eax,byte ptr [ebp-240]
	push	8240
	push	dword ptr [ebp+8]
	push	eax
	push	0
	call	near ptr MessageBoxA
	?debug	L 380
@91@409:
@92:
@92@0:
	pop	ebx
	leave
	ret
_ltk_dlg	endp
	assume	cs:_TEXT
_ltk_beep	proc	near
   ;	
   ;	void ltk_beep()
   ;	
	?debug	L 387
@96@39:
	push	ebp
	mov	ebp,esp
   ;	
   ;	{
   ;	  MessageBeep(MB_ICONEXCLAMATION);
   ;	
	?debug	L 389
@97@0:
	push	48
	call	near ptr MessageBeep
	?debug	L 390
@97@38:
@98:
@98@0:
	pop	ebp
	ret
_ltk_beep	endp
	assume	cs:_TEXT
_ltk_beg_wait	proc	near
   ;	
   ;	void ltk_beg_wait()
   ;	
	?debug	L 396
@99@43:
	push	ebp
	mov	ebp,esp
	?debug	L 398
@100@0:
@101:
@101@0:
	pop	ebp
	ret
_ltk_beg_wait	endp
	assume	cs:_TEXT
_ltk_end_wait	proc	near
   ;	
   ;	void ltk_end_wait()
   ;	
	?debug	L 405
@103@43:
	push	ebp
	mov	ebp,esp
	?debug	L 407
@104@0:
@105:
@105@0:
	pop	ebp
	ret
_ltk_end_wait	endp
_TEXT	ends
	?debug	D "d:winsvc.h" 7370 8256
	?debug	D "d:ole.h" 7370 8256
	?debug	D "d:winspool.h" 7370 8256
	?debug	D "d:drivinit.h" 7370 8256
	?debug	D "d:commdlg.h" 7370 8256
	?debug	D "d:winsock.h" 7370 8256
	?debug	D "d:winperf.h" 7370 8256
	?debug	D "d:shellapi.h" 7370 8256
	?debug	D "d:nb30.h" 7370 8256
	?debug	D "d:mmsystem.h" 7370 8256
	?debug	D "d:lzexpand.h" 7370 8256
	?debug	D "d:dlgs.h" 7370 8256
	?debug	D "d:ddeml.h" 7370 8256
	?debug	D "d:windef.h" 7370 8256
	?debug	D "d:dde.h" 7370 8256
	?debug	D "d:cderr.h" 7370 8256
	?debug	D "d:winnetwk.h" 7370 8256
	?debug	D "d:winreg.h" 7370 8256
	?debug	D "d:winver.h" 7370 8256
	?debug	D "d:wincon.h" 7370 8256
	?debug	D "d:mmsystem.h" 7370 8256
	?debug	D "d:winmm.h" 7370 8256
	?debug	D "d:winnls.h" 7370 8256
	?debug	D "d:winuser.h" 7370 8256
	?debug	D "d:wingdi.h" 7370 8256
	?debug	D "d:winerror.h" 7370 8256
	?debug	D "d:winbase.h" 7370 8256
	?debug	D "d:ctype.h" 7370 8256
	?debug	D "d:winnt.h" 7370 8256
	?debug	D "d:windef.h" 7370 8256
	?debug	D "d:stdarg.h" 7370 8256
	?debug	D "d:excpt.h" 7370 8256
	?debug	D "d:windows.h" 7370 8256
	?debug	D "d:assert.h" 7370 8256
	?debug	D "d:setjmp.h" 7370 8256
	?debug	D "los.h" 7396 33110
	?debug	D ".\ler.h" 6736 33664
	?debug	D "d:stddef.h" 7370 8256
	?debug	D "d:stdio.h" 7370 8256
	?debug	D "d:stdlib.h" 7370 8256
	?debug	D "d:dir.h" 7370 8256
	?debug	D "d:direct.h" 7370 8256
	?debug	D "d:dos.h" 7370 8256
	?debug	D "los.h" 7396 33110
	?debug	D "d:stdlib.h" 7370 8256
	?debug	D "d:string.h" 7370 8256
	?debug	D ".\lib.h" 6690 37184
	?debug	D ".\ltk.h" 6767 23939
	?debug	D "d:string.h" 7370 8256
	?debug	D "d:_null.h" 7370 8256
	?debug	D "d:_nfile.h" 7370 8256
	?debug	D "d:stdio.h" 7370 8256
	?debug	D "d:_defs.h" 7370 8256
	?debug	D "d:stdarg.h" 7370 8256
	?debug	D "ltkwin.c" 7396 32770
_s@	equ	s@
_RCSid	equ	RCSid
	extrn	_vsprintf:near
	extrn	_strncpy:near
	public	_ltkini
	public	_ltkfre
	public	_ltk_dlg
	public	_ltk_errlog
	public	_ltk_alloc
	public	_ltk_sigalloc
	public	_ltk_free
	public	_ltk_suballoc
	public	_ltk_sigsuballoc
	public	_ltk_subfree
	public	_ltk_beep
	public	_ltk_beg_wait
	public	_ltk_end_wait
	extrn	_errsigf:near
	extrn	_errfmt:near
	extrn	_errmsg:near
	extrn	GlobalAlloc:near
	extrn	GlobalSize:near
	extrn	GlobalLock:near
	extrn	GlobalUnlock:near
	extrn	GlobalFree:near
	extrn	LocalAlloc:near
	extrn	LocalLock:near
	extrn	LocalUnlock:near
	extrn	LocalFree:near
	extrn	MessageBoxA:near
	extrn	MessageBeep:near
_ltkseg	equ	ltkseg
_ltkhdl	equ	ltkhdl
	extrn	_LocalInit:near
_DATA	segment dword public use32 'DATA'
d@	label	byte
d@w	label	word
d@d	label	dword
RCSid	label	byte
	db	36
	db	72
	db	101
	db	97
	db	100
	db	101
	db	114
	db	36
	db	0
	db	3 dup(?)
ltkseg	label	dword
	dd	0
ltkhdl	label	dword
	dd	0
s@	label	byte
	db	"LTK",0,"LTK",0,"Error",0
_DATA	ends
_BSS	segment dword public use32 'BSS'
b@	label	byte
b@w	label	word
b@d	label	dword
_BSS	ends
$$BSYMS	segment byte public use32 'DEBSYM'
d8192@	label	byte
	db	2
	db	0
	db	0
	db	0
	dw	54
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@18@0-@11@37
	dd	5
	dd	@18@0-@11@37-4
	df	_ltkini
	dw	0
	dw	4096
	dw	0
	dw	1
	dw	0
	dw	0
	dw	0
	db	7
	db	95
	db	108
	db	116
	db	107
	db	105
	db	110
	db	105
	dw	18
	dw	512
	dw	8
	dw	0
	dw	117
	dw	0
	dw	2
	dw	0
	dw	0
	dw	0
	dw	14
	dw	529
	dw	1
	dd	@12@28-@11@37
	dd	@18@0-@12@28
	dw	24
	dw	16
	dw	2
	dw	1027
	dw	0
	dw	23
	dw	3
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	0
	dw	0
	dw	117
	dw	0
	dw	4
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	0
	dw	0
	dw	1027
	dw	0
	dw	5
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	0
	dw	0
	dw	1027
	dw	0
	dw	6
	dw	0
	dw	0
	dw	0
	dw	2
	dw	6
	dw	8
	dw	531
	dw	6
	dw	65528
	dw	65535
	dw	54
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@22@0-@20@37
	dd	3
	dd	@22@0-@20@37-2
	df	_ltkfre
	dw	0
	dw	4098
	dw	0
	dw	7
	dw	0
	dw	0
	dw	0
	db	7
	db	95
	db	108
	db	116
	db	107
	db	102
	db	114
	db	101
	dw	2
	dw	6
	dw	60
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@35@0-@24@43
	dd	5
	dd	@35@0-@24@43-4
	df	_ltk_suballoc
	dw	0
	dw	4100
	dw	0
	dw	8
	dw	0
	dw	0
	dw	0
	db	13
	db	95
	db	108
	db	116
	db	107
	db	95
	db	115
	db	117
	db	98
	db	97
	db	108
	db	108
	db	111
	db	99
	dw	18
	dw	512
	dw	8
	dw	0
	dw	117
	dw	0
	dw	9
	dw	0
	dw	0
	dw	0
	dw	16
	dw	2
	dw	4102
	dw	0
	dw	23
	dw	13
	dw	0
	dw	0
	dw	0
	dw	16
	dw	2
	dw	1027
	dw	0
	dw	24
	dw	14
	dw	0
	dw	0
	dw	0
	dw	2
	dw	6
	dw	8
	dw	531
	dw	6
	dw	65528
	dw	65535
	dw	63
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@43@0-@38@46
	dd	4
	dd	@43@0-@38@46-3
	df	_ltk_sigsuballoc
	dw	0
	dw	4106
	dw	0
	dw	57
	dw	0
	dw	0
	dw	0
	db	16
	db	95
	db	108
	db	116
	db	107
	db	95
	db	115
	db	105
	db	103
	db	115
	db	117
	db	98
	db	97
	db	108
	db	108
	db	111
	db	99
	dw	18
	dw	512
	dw	8
	dw	0
	dw	4107
	dw	0
	dw	58
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	12
	dw	0
	dw	117
	dw	0
	dw	59
	dw	0
	dw	0
	dw	0
	dw	16
	dw	2
	dw	1027
	dw	0
	dw	24
	dw	60
	dw	0
	dw	0
	dw	0
	dw	2
	dw	6
	dw	8
	dw	531
	dw	2
	dw	65532
	dw	65535
	dw	59
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@56@0-@51@42
	dd	4
	dd	@56@0-@51@42-3
	df	_ltk_subfree
	dw	0
	dw	4131
	dw	0
	dw	61
	dw	0
	dw	0
	dw	0
	db	12
	db	95
	db	108
	db	116
	db	107
	db	95
	db	115
	db	117
	db	98
	db	102
	db	114
	db	101
	db	101
	dw	18
	dw	512
	dw	8
	dw	0
	dw	1027
	dw	0
	dw	62
	dw	0
	dw	0
	dw	0
	dw	16
	dw	2
	dw	4102
	dw	0
	dw	24
	dw	63
	dw	0
	dw	0
	dw	0
	dw	2
	dw	6
	dw	8
	dw	531
	dw	2
	dw	65532
	dw	65535
	dw	57
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@67@0-@58@40
	dd	5
	dd	@67@0-@58@40-4
	df	_ltk_alloc
	dw	0
	dw	4133
	dw	0
	dw	64
	dw	0
	dw	0
	dw	0
	db	10
	db	95
	db	108
	db	116
	db	107
	db	95
	db	97
	db	108
	db	108
	db	111
	db	99
	dw	18
	dw	512
	dw	8
	dw	0
	dw	117
	dw	0
	dw	65
	dw	0
	dw	0
	dw	0
	dw	16
	dw	2
	dw	4102
	dw	0
	dw	23
	dw	66
	dw	0
	dw	0
	dw	0
	dw	16
	dw	2
	dw	1027
	dw	0
	dw	24
	dw	67
	dw	0
	dw	0
	dw	0
	dw	2
	dw	6
	dw	8
	dw	531
	dw	6
	dw	65528
	dw	65535
	dw	60
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@75@0-@70@43
	dd	4
	dd	@75@0-@70@43-3
	df	_ltk_sigalloc
	dw	0
	dw	4135
	dw	0
	dw	68
	dw	0
	dw	0
	dw	0
	db	13
	db	95
	db	108
	db	116
	db	107
	db	95
	db	115
	db	105
	db	103
	db	97
	db	108
	db	108
	db	111
	db	99
	dw	18
	dw	512
	dw	8
	dw	0
	dw	4107
	dw	0
	dw	69
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	12
	dw	0
	dw	117
	dw	0
	dw	70
	dw	0
	dw	0
	dw	0
	dw	16
	dw	2
	dw	1027
	dw	0
	dw	24
	dw	71
	dw	0
	dw	0
	dw	0
	dw	2
	dw	6
	dw	8
	dw	531
	dw	2
	dw	65532
	dw	65535
	dw	56
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@80@0-@77@39
	dd	4
	dd	@80@0-@77@39-3
	df	_ltk_free
	dw	0
	dw	4137
	dw	0
	dw	72
	dw	0
	dw	0
	dw	0
	db	9
	db	95
	db	108
	db	116
	db	107
	db	95
	db	102
	db	114
	db	101
	db	101
	dw	18
	dw	512
	dw	8
	dw	0
	dw	1027
	dw	0
	dw	73
	dw	0
	dw	0
	dw	0
	dw	16
	dw	2
	dw	4102
	dw	0
	dw	24
	dw	74
	dw	0
	dw	0
	dw	0
	dw	2
	dw	6
	dw	8
	dw	531
	dw	2
	dw	65532
	dw	65535
	dw	58
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@87@0-@83@41
	dd	5
	dd	@87@0-@83@41-3
	df	_ltk_errlog
	dw	0
	dw	4139
	dw	0
	dw	75
	dw	0
	dw	0
	dw	0
	db	11
	db	95
	db	108
	db	116
	db	107
	db	95
	db	101
	db	114
	db	114
	db	108
	db	111
	db	103
	dw	18
	dw	512
	dw	8
	dw	0
	dw	1027
	dw	0
	dw	76
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	12
	dw	0
	dw	1040
	dw	0
	dw	77
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	16
	dw	0
	dw	116
	dw	0
	dw	78
	dw	0
	dw	0
	dw	0
	dw	14
	dw	529
	dw	1
	dd	@84@29-@83@41
	dd	@87@0-@84@29
	dw	24
	dw	18
	dw	512
	dw	20
	dw	0
	dw	116
	dw	0
	dw	79
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	24
	dw	0
	dw	4140
	dw	0
	dw	80
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65408
	dw	65535
	dw	4142
	dw	0
	dw	81
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65280
	dw	65535
	dw	4143
	dw	0
	dw	82
	dw	0
	dw	0
	dw	0
	dw	2
	dw	6
	dw	8
	dw	531
	dw	2
	dw	65276
	dw	65535
	dw	55
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@92@0-@90@38
	dd	5
	dd	@92@0-@90@38-3
	df	_ltk_dlg
	dw	0
	dw	4144
	dw	0
	dw	83
	dw	0
	dw	0
	dw	0
	db	8
	db	95
	db	108
	db	116
	db	107
	db	95
	db	100
	db	108
	db	103
	dw	18
	dw	512
	dw	8
	dw	0
	dw	1040
	dw	0
	dw	84
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	12
	dw	0
	dw	1040
	dw	0
	dw	85
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65296
	dw	65535
	dw	4146
	dw	0
	dw	86
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65456
	dw	65535
	dw	4147
	dw	0
	dw	87
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	0
	dw	0
	dw	1027
	dw	0
	dw	88
	dw	0
	dw	0
	dw	0
	dw	14
	dw	529
	dw	1
	dd	@91@166-@90@38
	dd	@91@256-@91@166
	dw	17
	dw	2
	dw	6
	dw	8
	dw	531
	dw	1
	dw	65292
	dw	65535
	dw	56
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@98@0-@96@39
	dd	3
	dd	@98@0-@96@39-2
	df	_ltk_beep
	dw	0
	dw	4148
	dw	0
	dw	89
	dw	0
	dw	0
	dw	0
	db	9
	db	95
	db	108
	db	116
	db	107
	db	95
	db	98
	db	101
	db	101
	db	112
	dw	2
	dw	6
	dw	60
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@101@0-@99@43
	dd	3
	dd	@101@0-@99@43-2
	df	_ltk_beg_wait
	dw	0
	dw	4150
	dw	0
	dw	90
	dw	0
	dw	0
	dw	0
	db	13
	db	95
	db	108
	db	116
	db	107
	db	95
	db	98
	db	101
	db	103
	db	95
	db	119
	db	97
	db	105
	db	116
	dw	2
	dw	6
	dw	60
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	@105@0-@103@43
	dd	3
	dd	@105@0-@103@43-2
	df	_ltk_end_wait
	dw	0
	dw	4152
	dw	0
	dw	91
	dw	0
	dw	0
	dw	0
	db	13
	db	95
	db	108
	db	116
	db	107
	db	95
	db	101
	db	110
	db	100
	db	95
	db	119
	db	97
	db	105
	db	116
	dw	2
	dw	6
	dw	22
	dw	513
	df	DGROUP:RCSid
	dw	0
	dw	4154
	dw	0
	dw	92
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	93
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	94
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	95
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4124
	dw	0
	dw	0
	dw	96
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	97
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	98
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	3
	dw	0
	dw	0
	dw	99
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4124
	dw	0
	dw	0
	dw	100
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	101
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	102
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	103
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	104
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	105
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	16
	dw	0
	dw	0
	dw	106
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	16
	dw	0
	dw	0
	dw	107
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	108
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	109
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	110
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	111
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	112
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	113
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	114
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4113
	dw	0
	dw	1
	dw	115
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4108
	dw	0
	dw	1
	dw	116
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4116
	dw	0
	dw	1
	dw	117
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4115
	dw	0
	dw	0
	dw	118
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4113
	dw	0
	dw	0
	dw	119
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4110
	dw	0
	dw	1
	dw	120
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4110
	dw	0
	dw	0
	dw	121
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4127
	dw	0
	dw	1
	dw	122
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4127
	dw	0
	dw	0
	dw	123
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4108
	dw	0
	dw	0
	dw	124
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	125
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	126
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	127
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	128
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	129
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	130
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	131
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	132
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	133
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	64
	dw	0
	dw	0
	dw	134
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	135
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	136
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	137
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	138
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	139
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	140
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	16
	dw	0
	dw	0
	dw	141
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	17
	dw	0
	dw	0
	dw	142
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	143
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	144
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	145
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	146
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	147
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4156
	dw	0
	dw	0
	dw	148
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4156
	dw	0
	dw	0
	dw	149
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	150
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	151
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	152
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4156
	dw	0
	dw	0
	dw	153
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4156
	dw	0
	dw	0
	dw	154
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	16
	dw	0
	dw	0
	dw	155
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	156
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	157
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	158
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	159
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	160
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	161
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	162
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4156
	dw	0
	dw	0
	dw	163
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	164
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	16
	dw	0
	dw	0
	dw	165
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	166
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	167
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	65
	dw	0
	dw	0
	dw	168
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	65
	dw	0
	dw	0
	dw	169
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	170
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	171
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	172
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	173
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	174
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	175
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	176
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	177
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	178
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	179
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	180
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	181
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	182
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	183
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	184
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	185
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	186
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	187
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	188
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	189
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	190
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	191
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	192
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	193
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	194
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	195
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	196
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	197
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	198
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	199
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	200
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	201
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	202
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	203
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	204
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	205
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	206
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	207
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	208
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	209
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	210
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	211
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	212
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	213
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	214
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	3
	dw	0
	dw	0
	dw	215
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	3
	dw	0
	dw	0
	dw	216
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	3
	dw	0
	dw	0
	dw	217
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	218
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	219
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	220
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	221
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	222
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	223
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	224
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	225
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	226
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	227
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	228
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	229
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	230
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	231
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	232
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	233
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	234
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1040
	dw	0
	dw	0
	dw	235
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	236
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	237
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	238
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	239
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	240
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	241
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	242
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	243
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	244
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	245
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	246
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	247
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	248
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	249
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	250
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	251
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	252
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	253
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	254
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	255
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	256
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4103
	dw	0
	dw	1
	dw	257
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4103
	dw	0
	dw	0
	dw	258
	dw	0
	dw	0
	dw	0
	dw	22
	dw	513
	df	DGROUP:ltkseg
	dw	0
	dw	117
	dw	0
	dw	259
	dw	0
	dw	0
	dw	0
	dw	22
	dw	513
	df	DGROUP:ltkhdl
	dw	0
	dw	1027
	dw	0
	dw	260
	dw	0
	dw	0
	dw	0
	dw	13
	dw	1
	db	2
	db	0
	db	8
	db	24
	db	6
	db	66
	db	67
	db	52
	db	46
	db	48
	db	48
$$BSYMS	ends
$$BTYPES	segment byte public use32 'DEBTYP'
d8193@	label	byte
	dd	2
	dd	524302
	dd	3
	dd	65536
	dd	4097
	dd	33619976
	dd	7667713
	dd	917504
	dd	196616
	dd	0
	dd	268632064
	dd	262144
	dd	513
	dd	524302
	dd	1027
	dd	65536
	dd	4101
	dd	33619976
	dd	7667713
	dd	524288
	dd	655362
	dd	4103
	dd	327708
	dd	269025282
	dd	0
	dd	0
	dd	0
	dd	0
	dd	10
	dd	1179653
	dd	2097155
	dd	1114112
	dd	0
	dd	65536
	dd	2621441
	dd	67502596
	dd	1027
	dd	720896
	dd	0
	dd	0
	dd	67564018
	dd	4104
	dd	786432
	dd	0
	dd	262144
	dd	524302
	dd	1027
	dd	131072
	dd	4130
	dd	131080
	dd	269221898
	dd	1835008
	dd	589829
	dd	4129
	dd	0
	dd	0
	dd	0
	dd	983040
	dd	35651584
	dd	131080
	dd	269352970
	dd	1835008
	dd	393221
	dd	4118
	dd	0
	dd	0
	dd	0
	dd	1048576
	dd	5963776
	dd	196626
	dd	16
	dd	17
	dd	0
	dd	458759
	dd	196626
	dd	4113
	dd	17
	dd	0
	dd	655400
	dd	393236
	dd	269615106
	dd	0
	dd	0
	dd	17
	dd	2621444
	dd	67502596
	dd	116
	dd	1179648
	dd	0
	dd	0
	dd	67564018
	dd	1040
	dd	1245184
	dd	0
	dd	0
	dd	196626
	dd	4116
	dd	17
	dd	0
	dd	65568
	dd	327708
	dd	269811720
	dd	0
	dd	0
	dd	0
	dd	0
	dd	20
	dd	10485792
	dd	67502596
	dd	117
	dd	1376256
	dd	0
	dd	0
	dd	67564018
	dd	117
	dd	1441792
	dd	0
	dd	262144
	dd	67564018
	dd	117
	dd	1507328
	dd	0
	dd	524288
	dd	67564018
	dd	117
	dd	1572864
	dd	0
	dd	786432
	dd	67564018
	dd	117
	dd	1638400
	dd	0
	dd	1048576
	dd	67564018
	dd	117
	dd	1703936
	dd	0
	dd	1310720
	dd	67564018
	dd	117
	dd	1769472
	dd	0
	dd	1572864
	dd	67564018
	dd	117
	dd	1835008
	dd	0
	dd	1835008
	dd	33816696
	dd	269288454
	dd	0
	dd	29
	dd	0
	dd	4059168768
	dd	7603206
	dd	0
	dd	30
	dd	0
	dd	4059168772
	dd	269419526
	dd	0
	dd	31
	dd	0
	dd	4059168776
	dd	269485062
	dd	0
	dd	32
	dd	0
	dd	4059168783
	dd	7603206
	dd	0
	dd	33
	dd	0
	dd	4059168823
	dd	269681670
	dd	0
	dd	34
	dd	0
	dd	524347
	dd	655362
	dd	4120
	dd	524302
	dd	3
	dd	0
	dd	4121
	dd	33619972
	dd	1179648
	dd	1048579
	dd	1114112
	dd	0
	dd	33554432
	dd	524800
	dd	655362
	dd	4124
	dd	327708
	dd	270336009
	dd	0
	dd	0
	dd	0
	dd	0
	dd	35
	dd	11796504
	dd	67502596
	dd	1056
	dd	2359296
	dd	0
	dd	0
	dd	67564018
	dd	1056
	dd	2424832
	dd	0
	dd	262144
	dd	67564018
	dd	116
	dd	2490368
	dd	0
	dd	524288
	dd	67564018
	dd	116
	dd	2555904
	dd	0
	dd	786432
	dd	67564018
	dd	33
	dd	2621440
	dd	0
	dd	1048576
	dd	67564018
	dd	33
	dd	2686976
	dd	0
	dd	1179648
	dd	67564018
	dd	17
	dd	2752512
	dd	0
	dd	1310720
	dd	67564018
	dd	16
	dd	2818048
	dd	0
	dd	1441792
	dd	67564018
	dd	32
	dd	2883584
	dd	0
	dd	1507328
	dd	131080
	dd	270467082
	dd	1835008
	dd	131077
	dd	4128
	dd	0
	dd	0
	dd	0
	dd	2949120
	dd	524288
	dd	33816616
	dd	7668742
	dd	0
	dd	46
	dd	0
	dd	4059168768
	dd	2229254
	dd	0
	dd	47
	dd	0
	dd	11796484
	dd	67502596
	dd	4109
	dd	3145728
	dd	0
	dd	0
	dd	67564018
	dd	4119
	dd	3211264
	dd	0
	dd	262144
	dd	67564018
	dd	1027
	dd	3276800
	dd	0
	dd	524288
	dd	67564018
	dd	116
	dd	3342336
	dd	0
	dd	786432
	dd	67564018
	dd	4122
	dd	3407872
	dd	0
	dd	1048576
	dd	67564018
	dd	4123
	dd	3473408
	dd	0
	dd	34603008
	dd	67564018
	dd	4126
	dd	3538944
	dd	0
	dd	34865152
	dd	67564018
	dd	117
	dd	3604480
	dd	0
	dd	35127296
	dd	67564018
	dd	34
	dd	3670016
	dd	0
	dd	35389440
	dd	33619980
	dd	269156354
	dd	7667712
	dd	917504
	dd	196616
	dd	0
	dd	270794753
	dd	524288
	dd	66049
	dd	1027
	dd	524302
	dd	1027
	dd	65536
	dd	4134
	dd	33619976
	dd	7667713
	dd	917504
	dd	67305480
	dd	0
	dd	271056898
	dd	786432
	dd	131585
	dd	4107
	dd	117
	dd	524302
	dd	3
	dd	65536
	dd	4138
	dd	33619976
	dd	67305473
	dd	917504
	dd	196616
	dd	0
	dd	271384581
	dd	524288
	dd	655362
	dd	4113
	dd	33619992
	dd	67305477
	dd	68157440
	dd	7602176
	dd	7602176
	dd	271319040
	dd	1179648
	dd	1048579
	dd	1114112
	dd	0
	dd	8388608
	dd	1179776
	dd	1048579
	dd	1114112
	dd	0
	dd	8388608
	dd	917632
	dd	196616
	dd	4194304
	dd	271646723
	dd	1048576
	dd	197121
	dd	1040
	dd	1040
	dd	0
	dd	196626
	dd	16
	dd	17
	dd	0
	dd	10485920
	dd	196626
	dd	16
	dd	17
	dd	0
	dd	5242960
	dd	524302
	dd	3
	dd	0
	dd	4149
	dd	33619972
	dd	917504
	dd	196616
	dd	0
	dd	272039936
	dd	262144
	dd	513
	dd	524302
	dd	3
	dd	0
	dd	4153
	dd	33619972
	dd	1179648
	dd	1048579
	dd	1114112
	dd	0
	dd	589824
	dd	917513
	dd	7602184
	dd	0
	dd	272498691
	dd	524288
	dd	655362
	dd	4157
	dd	65544
	dd	1048577
	dd	1048576
	dd	197121
	dd	1040
	dd	4156
	dd	1027
	dd	524302
	dd	1040
	dd	196608
	dd	4160
	dd	33619984
	dd	68157443
	dd	272367616
	dd	7667712
	dd	917504
	dd	196616
	dd	0
	dd	272760832
	dd	262144
	dd	513
	dd	524302
	dd	116
	dd	0
	dd	4164
	dd	33619972
	dd	917504
	dd	196616
	dd	0
	dd	273022976
	dd	262144
	dd	513
	dd	524302
	dd	1027
	dd	131079
	dd	4168
	dd	33619980
	dd	7667714
	dd	2228224
	dd	917504
	dd	2228232
	dd	458752
	dd	273285121
	dd	524288
	dd	66049
	dd	1027
	dd	524302
	dd	1027
	dd	65543
	dd	4172
	dd	33619976
	dd	67305473
	dd	917504
	dd	7602184
	dd	458752
	dd	273547265
	dd	524288
	dd	66049
	dd	1027
	dd	524302
	dd	1027
	dd	65543
	dd	4176
	dd	33619976
	dd	67305473
	dd	917504
	dd	67305480
	dd	458752
	dd	273809410
	dd	786432
	dd	131585
	dd	117
	dd	117
	dd	524302
	dd	1027
	dd	65543
	dd	4180
	dd	33619976
	dd	67305473
	dd	917504
	dd	7602184
	dd	458752
	dd	274071553
	dd	524288
	dd	66049
	dd	1027
	dd	524302
	dd	1027
	dd	65543
	dd	4184
	dd	33619976
	dd	67305473
	dd	917504
	dd	7602184
	dd	458752
	dd	274333700
	dd	1310720
	dd	262657
	dd	1027
	dd	4156
	dd	4156
	dd	117
	dd	524302
	dd	116
	dd	65543
	dd	4188
	dd	33619976
	dd	7667713
	dd	917504
	dd	7602184
	dd	0
	dd	274595840
	dd	262144
	dd	513
$$BTYPES	ends
$$BNAMES	segment byte public use32 'DEBNAM'
d8194@	label	byte
	db	6
	db	108
	db	116
	db	107
	db	105
	db	110
	db	105
	db	7
	db	104
	db	101
	db	97
	db	112
	db	115
	db	105
	db	122
	db	4
	db	104
	db	101
	db	97
	db	112
	db	4
	db	115
	db	101
	db	103
	db	100
	db	6
	db	115
	db	121
	db	115
	db	104
	db	100
	db	108
	db	6
	db	109
	db	101
	db	109
	db	104
	db	100
	db	108
	db	6
	db	108
	db	116
	db	107
	db	102
	db	114
	db	101
	db	12
	db	108
	db	116
	db	107
	db	95
	db	115
	db	117
	db	98
	db	97
	db	108
	db	108
	db	111
	db	99
	db	3
	db	115
	db	105
	db	122
	db	8
	db	108
	db	116
	db	107
	db	95
	db	109
	db	98
	db	108
	db	107
	db	7
	db	109
	db	98
	db	108
	db	107
	db	104
	db	100
	db	108
	db	7
	db	109
	db	98
	db	108
	db	107
	db	98
	db	117
	db	102
	db	4
	db	109
	db	98
	db	108
	db	107
	db	6
	db	109
	db	101
	db	109
	db	104
	db	100
	db	108
	db	8
	db	101
	db	114
	db	114
	db	99
	db	120
	db	100
	db	101
	db	102
	db	6
	db	101
	db	114
	db	114
	db	100
	db	101
	db	102
	db	7
	db	101
	db	114
	db	114
	db	97
	db	100
	db	101
	db	102
	db	7
	db	101
	db	114
	db	114
	db	97
	db	105
	db	110
	db	116
	db	7
	db	101
	db	114
	db	114
	db	97
	db	115
	db	116
	db	114
	db	9
	db	95
	db	95
	db	106
	db	109
	db	112
	db	95
	db	98
	db	117
	db	102
	db	5
	db	106
	db	95
	db	101
	db	98
	db	112
	db	5
	db	106
	db	95
	db	101
	db	98
	db	120
	db	5
	db	106
	db	95
	db	101
	db	100
	db	105
	db	5
	db	106
	db	95
	db	101
	db	115
	db	105
	db	5
	db	106
	db	95
	db	101
	db	115
	db	112
	db	5
	db	106
	db	95
	db	114
	db	101
	db	116
	db	7
	db	106
	db	95
	db	101
	db	120
	db	99
	db	101
	db	112
	db	9
	db	106
	db	95
	db	99
	db	111
	db	110
	db	116
	db	101
	db	120
	db	116
	db	6
	db	101
	db	114
	db	114
	db	112
	db	114
	db	118
	db	7
	db	101
	db	114
	db	114
	db	99
	db	111
	db	100
	db	101
	db	6
	db	101
	db	114
	db	114
	db	102
	db	97
	db	99
	db	6
	db	101
	db	114
	db	114
	db	97
	db	97
	db	118
	db	6
	db	101
	db	114
	db	114
	db	97
	db	97
	db	99
	db	6
	db	101
	db	114
	db	114
	db	98
	db	117
	db	102
	db	4
	db	70
	db	73
	db	76
	db	69
	db	4
	db	99
	db	117
	db	114
	db	112
	db	6
	db	98
	db	117
	db	102
	db	102
	db	101
	db	114
	db	5
	db	108
	db	101
	db	118
	db	101
	db	108
	db	5
	db	98
	db	115
	db	105
	db	122
	db	101
	db	6
	db	105
	db	115
	db	116
	db	101
	db	109
	db	112
	db	5
	db	102
	db	108
	db	97
	db	103
	db	115
	db	5
	db	116
	db	111
	db	107
	db	101
	db	110
	db	2
	db	102
	db	100
	db	4
	db	104
	db	111
	db	108
	db	100
	db	8
	db	101
	db	114
	db	114
	db	109
	db	102
	db	100
	db	101
	db	102
	db	8
	db	101
	db	114
	db	114
	db	109
	db	102
	db	110
	db	117
	db	109
	db	9
	db	101
	db	114
	db	114
	db	109
	db	102
	db	115
	db	101
	db	101
	db	107
	db	8
	db	101
	db	114
	db	114
	db	99
	db	120
	db	112
	db	116
	db	114
	db	8
	db	101
	db	114
	db	114
	db	99
	db	120
	db	108
	db	111
	db	103
	db	8
	db	101
	db	114
	db	114
	db	99
	db	120
	db	108
	db	103
	db	99
	db	8
	db	101
	db	114
	db	114
	db	99
	db	120
	db	111
	db	102
	db	115
	db	8
	db	101
	db	114
	db	114
	db	99
	db	120
	db	98
	db	117
	db	102
	db	7
	db	101
	db	114
	db	114
	db	99
	db	120
	db	102
	db	112
	db	9
	db	101
	db	114
	db	114
	db	99
	db	120
	db	115
	db	101
	db	101
	db	107
	db	9
	db	101
	db	114
	db	114
	db	99
	db	120
	db	115
	db	107
	db	115
	db	122
	db	9
	db	101
	db	114
	db	114
	db	99
	db	120
	db	98
	db	97
	db	115
	db	101
	db	15
	db	108
	db	116
	db	107
	db	95
	db	115
	db	105
	db	103
	db	115
	db	117
	db	98
	db	97
	db	108
	db	108
	db	111
	db	99
	db	5
	db	101
	db	114
	db	114
	db	99
	db	120
	db	3
	db	115
	db	105
	db	122
	db	3
	db	112
	db	116
	db	114
	db	11
	db	108
	db	116
	db	107
	db	95
	db	115
	db	117
	db	98
	db	102
	db	114
	db	101
	db	101
	db	3
	db	112
	db	116
	db	114
	db	4
	db	109
	db	98
	db	108
	db	107
	db	9
	db	108
	db	116
	db	107
	db	95
	db	97
	db	108
	db	108
	db	111
	db	99
	db	3
	db	115
	db	105
	db	122
	db	4
	db	109
	db	98
	db	108
	db	107
	db	6
	db	109
	db	101
	db	109
	db	104
	db	100
	db	108
	db	12
	db	108
	db	116
	db	107
	db	95
	db	115
	db	105
	db	103
	db	97
	db	108
	db	108
	db	111
	db	99
	db	5
	db	101
	db	114
	db	114
	db	99
	db	120
	db	3
	db	115
	db	105
	db	122
	db	3
	db	112
	db	116
	db	114
	db	8
	db	108
	db	116
	db	107
	db	95
	db	102
	db	114
	db	101
	db	101
	db	3
	db	109
	db	101
	db	109
	db	4
	db	109
	db	98
	db	108
	db	107
	db	10
	db	108
	db	116
	db	107
	db	95
	db	101
	db	114
	db	114
	db	108
	db	111
	db	103
	db	3
	db	99
	db	116
	db	120
	db	3
	db	102
	db	97
	db	99
	db	5
	db	101
	db	114
	db	114
	db	110
	db	111
	db	4
	db	97
	db	114
	db	103
	db	99
	db	4
	db	97
	db	114
	db	103
	db	118
	db	3
	db	109
	db	115
	db	103
	db	3
	db	98
	db	117
	db	102
	db	7
	db	108
	db	116
	db	107
	db	95
	db	100
	db	108
	db	103
	db	5
	db	116
	db	105
	db	116
	db	108
	db	101
	db	3
	db	109
	db	115
	db	103
	db	6
	db	111
	db	117
	db	116
	db	98
	db	117
	db	102
	db	5
	db	105
	db	110
	db	98
	db	117
	db	102
	db	4
	db	97
	db	114
	db	103
	db	112
	db	8
	db	108
	db	116
	db	107
	db	95
	db	98
	db	101
	db	101
	db	112
	db	12
	db	108
	db	116
	db	107
	db	95
	db	98
	db	101
	db	103
	db	95
	db	119
	db	97
	db	105
	db	116
	db	12
	db	108
	db	116
	db	107
	db	95
	db	101
	db	110
	db	100
	db	95
	db	119
	db	97
	db	105
	db	116
	db	5
	db	82
	db	67
	db	83
	db	105
	db	100
	db	7
	db	118
	db	97
	db	95
	db	108
	db	105
	db	115
	db	116
	db	6
	db	115
	db	105
	db	122
	db	101
	db	95
	db	116
	db	6
	db	102
	db	112
	db	111
	db	115
	db	95
	db	116
	db	4
	db	70
	db	73
	db	76
	db	69
	db	7
	db	119
	db	99
	db	104
	db	97
	db	114
	db	95
	db	116
	db	9
	db	112
	db	116
	db	114
	db	100
	db	105
	db	102
	db	102
	db	95
	db	116
	db	5
	db	100
	db	118
	db	111
	db	105
	db	100
	db	8
	db	111
	db	115
	db	102
	db	105
	db	108
	db	100
	db	101
	db	102
	db	5
	db	117
	db	99
	db	104
	db	97
	db	114
	db	6
	db	117
	db	115
	db	104
	db	111
	db	114
	db	116
	db	4
	db	117
	db	105
	db	110
	db	116
	db	5
	db	117
	db	108
	db	111
	db	110
	db	103
	db	3
	db	117
	db	98
	db	49
	db	3
	db	115
	db	98
	db	49
	db	2
	db	98
	db	49
	db	3
	db	117
	db	98
	db	50
	db	3
	db	115
	db	98
	db	50
	db	2
	db	98
	db	50
	db	3
	db	117
	db	98
	db	52
	db	3
	db	115
	db	98
	db	52
	db	2
	db	98
	db	52
	db	5
	db	101
	db	119
	db	111
	db	114
	db	100
	db	7
	db	101
	db	114
	db	114
	db	97
	db	100
	db	101
	db	102
	db	8
	db	101
	db	114
	db	114
	db	99
	db	120
	db	100
	db	101
	db	102
	db	9
	db	95
	db	95
	db	106
	db	109
	db	112
	db	95
	db	98
	db	117
	db	102
	db	7
	db	106
	db	109
	db	112
	db	95
	db	98
	db	117
	db	102
	db	7
	db	101
	db	114
	db	114
	db	97
	db	100
	db	101
	db	102
	db	6
	db	101
	db	114
	db	114
	db	100
	db	101
	db	102
	db	6
	db	101
	db	114
	db	114
	db	100
	db	101
	db	102
	db	8
	db	101
	db	114
	db	114
	db	109
	db	102
	db	100
	db	101
	db	102
	db	8
	db	101
	db	114
	db	114
	db	109
	db	102
	db	100
	db	101
	db	102
	db	8
	db	101
	db	114
	db	114
	db	99
	db	120
	db	100
	db	101
	db	102
	db	5
	db	85
	db	76
	db	79
	db	78
	db	71
	db	6
	db	85
	db	83
	db	72
	db	79
	db	82
	db	84
	db	5
	db	85
	db	67
	db	72
	db	65
	db	82
	db	6
	db	80
	db	85
	db	67
	db	72
	db	65
	db	82
	db	3
	db	80
	db	83
	db	90
	db	5
	db	68
	db	87
	db	79
	db	82
	db	68
	db	4
	db	66
	db	79
	db	79
	db	76
	db	4
	db	66
	db	89
	db	84
	db	69
	db	4
	db	87
	db	79
	db	82
	db	68
	db	5
	db	70
	db	76
	db	79
	db	65
	db	84
	db	5
	db	80
	db	66
	db	89
	db	84
	db	69
	db	6
	db	76
	db	80
	db	66
	db	89
	db	84
	db	69
	db	6
	db	76
	db	80
	db	86
	db	79
	db	73
	db	68
	db	3
	db	73
	db	78
	db	84
	db	4
	db	85
	db	73
	db	78
	db	84
	db	5
	db	80
	db	86
	db	79
	db	73
	db	68
	db	4
	db	67
	db	72
	db	65
	db	82
	db	5
	db	83
	db	72
	db	79
	db	82
	db	84
	db	4
	db	76
	db	79
	db	78
	db	71
	db	5
	db	87
	db	67
	db	72
	db	65
	db	82
	db	5
	db	80
	db	67
	db	72
	db	65
	db	82
	db	4
	db	76
	db	80
	db	67
	db	72
	db	3
	db	80
	db	67
	db	72
	db	5
	db	76
	db	80
	db	67
	db	67
	db	72
	db	4
	db	80
	db	67
	db	67
	db	72
	db	5
	db	78
	db	80
	db	83
	db	84
	db	82
	db	5
	db	76
	db	80
	db	83
	db	84
	db	82
	db	4
	db	80
	db	83
	db	84
	db	82
	db	6
	db	76
	db	80
	db	67
	db	83
	db	84
	db	82
	db	5
	db	80
	db	67
	db	83
	db	84
	db	82
	db	5
	db	84
	db	67
	db	72
	db	65
	db	82
	db	6
	db	80
	db	84
	db	67
	db	72
	db	65
	db	82
	db	5
	db	84
	db	66
	db	89
	db	84
	db	69
	db	6
	db	80
	db	84
	db	66
	db	89
	db	84
	db	69
	db	5
	db	76
	db	80
	db	84
	db	67
	db	72
	db	4
	db	80
	db	84
	db	67
	db	72
	db	5
	db	80
	db	84
	db	83
	db	84
	db	82
	db	6
	db	76
	db	80
	db	84
	db	83
	db	84
	db	82
	db	7
	db	76
	db	80
	db	67
	db	84
	db	83
	db	84
	db	82
	db	6
	db	72
	db	65
	db	78
	db	68
	db	76
	db	69
	db	5
	db	67
	db	67
	db	72
	db	65
	db	82
	db	4
	db	76
	db	67
	db	73
	db	68
	db	6
	db	76
	db	65
	db	78
	db	71
	db	73
	db	68
	db	8
	db	76
	db	79
	db	78
	db	71
	db	76
	db	79
	db	78
	db	71
	db	9
	db	68
	db	87
	db	79
	db	82
	db	68
	db	76
	db	79
	db	78
	db	71
	db	7
	db	66
	db	79
	db	79
	db	76
	db	69
	db	65
	db	78
	db	8
	db	80
	db	66
	db	79
	db	79
	db	76
	db	69
	db	65
	db	78
	db	10
	db	75
	db	83
	db	80
	db	73
	db	78
	db	95
	db	76
	db	79
	db	67
	db	75
	db	13
	db	80
	db	65
	db	67
	db	67
	db	69
	db	83
	db	83
	db	95
	db	84
	db	79
	db	75
	db	69
	db	78
	db	20
	db	80
	db	83
	db	69
	db	67
	db	85
	db	82
	db	73
	db	84
	db	89
	db	95
	db	68
	db	69
	db	83
	db	67
	db	82
	db	73
	db	80
	db	84
	db	79
	db	82
	db	4
	db	80
	db	83
	db	73
	db	68
	db	11
	db	65
	db	67
	db	67
	db	69
	db	83
	db	83
	db	95
	db	77
	db	65
	db	83
	db	75
	db	27
	db	83
	db	69
	db	67
	db	85
	db	82
	db	73
	db	84
	db	89
	db	95
	db	68
	db	69
	db	83
	db	67
	db	82
	db	73
	db	80
	db	84
	db	79
	db	82
	db	95
	db	67
	db	79
	db	78
	db	84
	db	82
	db	79
	db	76
	db	30
	db	83
	db	69
	db	67
	db	85
	db	82
	db	73
	db	84
	db	89
	db	95
	db	67
	db	79
	db	78
	db	84
	db	69
	db	88
	db	84
	db	95
	db	84
	db	82
	db	65
	db	67
	db	75
	db	73
	db	78
	db	71
	db	95
	db	77
	db	79
	db	68
	db	69
	db	31
	db	80
	db	83
	db	69
	db	67
	db	85
	db	82
	db	73
	db	84
	db	89
	db	95
	db	67
	db	79
	db	78
	db	84
	db	69
	db	88
	db	84
	db	95
	db	84
	db	82
	db	65
	db	67
	db	75
	db	73
	db	78
	db	71
	db	95
	db	77
	db	79
	db	68
	db	69
	db	20
	db	83
	db	69
	db	67
	db	85
	db	82
	db	73
	db	84
	db	89
	db	95
	db	73
	db	78
	db	70
	db	79
	db	82
	db	77
	db	65
	db	84
	db	73
	db	79
	db	78
	db	6
	db	87
	db	80
	db	65
	db	82
	db	65
	db	77
	db	6
	db	76
	db	80
	db	65
	db	82
	db	65
	db	77
	db	7
	db	76
	db	82
	db	69
	db	83
	db	85
	db	76
	db	84
	db	4
	db	72
	db	87
	db	78
	db	68
	db	5
	db	72
	db	72
	db	79
	db	79
	db	75
	db	4
	db	65
	db	84
	db	79
	db	77
	db	7
	db	72
	db	71
	db	76
	db	79
	db	66
	db	65
	db	76
	db	6
	db	72
	db	76
	db	79
	db	67
	db	65
	db	76
	db	12
	db	71
	db	76
	db	79
	db	66
	db	65
	db	76
	db	72
	db	65
	db	78
	db	68
	db	76
	db	69
	db	11
	db	76
	db	79
	db	67
	db	65
	db	76
	db	72
	db	65
	db	78
	db	68
	db	76
	db	69
	db	7
	db	72
	db	71
	db	68
	db	73
	db	79
	db	66
	db	74
	db	6
	db	72
	db	65
	db	67
	db	67
	db	69
	db	76
	db	7
	db	72
	db	66
	db	73
	db	84
	db	77
	db	65
	db	80
	db	6
	db	72
	db	66
	db	82
	db	85
	db	83
	db	72
	db	3
	db	72
	db	68
	db	67
	db	5
	db	72
	db	68
	db	69
	db	83
	db	75
	db	12
	db	72
	db	69
	db	78
	db	72
	db	77
	db	69
	db	84
	db	65
	db	70
	db	73
	db	76
	db	69
	db	5
	db	72
	db	70
	db	79
	db	78
	db	84
	db	5
	db	72
	db	73
	db	67
	db	79
	db	78
	db	5
	db	72
	db	77
	db	69
	db	78
	db	85
	db	9
	db	72
	db	77
	db	69
	db	84
	db	65
	db	70
	db	73
	db	76
	db	69
	db	9
	db	72
	db	73
	db	78
	db	83
	db	84
	db	65
	db	78
	db	67
	db	69
	db	7
	db	72
	db	77
	db	79
	db	68
	db	85
	db	76
	db	69
	db	8
	db	72
	db	80
	db	65
	db	76
	db	69
	db	84
	db	84
	db	69
	db	4
	db	72
	db	80
	db	69
	db	78
	db	4
	db	72
	db	82
	db	71
	db	78
	db	5
	db	72
	db	82
	db	83
	db	82
	db	67
	db	4
	db	72
	db	83
	db	84
	db	82
	db	7
	db	72
	db	87
	db	73
	db	78
	db	83
	db	84
	db	65
	db	3
	db	72
	db	75
	db	76
	db	5
	db	72
	db	70
	db	73
	db	76
	db	69
	db	7
	db	72
	db	67
	db	85
	db	82
	db	83
	db	79
	db	82
	db	8
	db	67
	db	79
	db	76
	db	79
	db	82
	db	82
	db	69
	db	70
	db	4
	db	72
	db	68
	db	87
	db	80
	db	13
	db	77
	db	69
	db	78
	db	85
	db	84
	db	69
	db	77
	db	80
	db	76
	db	65
	db	84
	db	69
	db	65
	db	13
	db	77
	db	69
	db	78
	db	85
	db	84
	db	69
	db	77
	db	80
	db	76
	db	65
	db	84
	db	69
	db	87
	db	12
	db	77
	db	69
	db	78
	db	85
	db	84
	db	69
	db	77
	db	80
	db	76
	db	65
	db	84
	db	69
	db	15
	db	76
	db	80
	db	77
	db	69
	db	78
	db	85
	db	84
	db	69
	db	77
	db	80
	db	76
	db	65
	db	84
	db	69
	db	65
	db	15
	db	76
	db	80
	db	77
	db	69
	db	78
	db	85
	db	84
	db	69
	db	77
	db	80
	db	76
	db	65
	db	84
	db	69
	db	87
	db	14
	db	76
	db	80
	db	77
	db	69
	db	78
	db	85
	db	84
	db	69
	db	77
	db	80
	db	76
	db	65
	db	84
	db	69
	db	8
	db	72
	db	69
	db	76
	db	80
	db	80
	db	79
	db	76
	db	89
	db	6
	db	76
	db	67
	db	84
	db	89
	db	80
	db	69
	db	9
	db	77
	db	77
	db	86
	db	69
	db	82
	db	83
	db	73
	db	79
	db	78
	db	8
	db	77
	db	77
	db	82
	db	69
	db	83
	db	85
	db	76
	db	84
	db	8
	db	77
	db	67
	db	73
	db	69
	db	82
	db	82
	db	79
	db	82
	db	5
	db	72
	db	68
	db	82
	db	86
	db	82
	db	5
	db	72
	db	87
	db	65
	db	86
	db	69
	db	7
	db	72
	db	87
	db	65
	db	86
	db	69
	db	73
	db	78
	db	8
	db	72
	db	87
	db	65
	db	86
	db	69
	db	79
	db	85
	db	84
	db	5
	db	72
	db	77
	db	73
	db	68
	db	73
	db	7
	db	72
	db	77
	db	73
	db	68
	db	73
	db	73
	db	78
	db	8
	db	72
	db	77
	db	73
	db	68
	db	73
	db	79
	db	85
	db	84
	db	6
	db	70
	db	79
	db	85
	db	82
	db	67
	db	67
	db	5
	db	72
	db	77
	db	77
	db	73
	db	79
	db	5
	db	72
	db	80
	db	83
	db	84
	db	82
	db	11
	db	77
	db	67
	db	73
	db	68
	db	69
	db	86
	db	73
	db	67
	db	69
	db	73
	db	68
	db	6
	db	82
	db	69
	db	71
	db	83
	db	65
	db	77
	db	4
	db	72
	db	75
	db	69
	db	89
	db	9
	db	72
	db	67
	db	79
	db	78
	db	86
	db	76
	db	73
	db	83
	db	84
	db	5
	db	72
	db	67
	db	79
	db	78
	db	86
	db	3
	db	72
	db	83
	db	90
	db	8
	db	72
	db	68
	db	68
	db	69
	db	68
	db	65
	db	84
	db	65
	db	5
	db	72
	db	68
	db	82
	db	79
	db	80
	db	6
	db	117
	db	95
	db	99
	db	104
	db	97
	db	114
	db	7
	db	117
	db	95
	db	115
	db	104
	db	111
	db	114
	db	116
	db	5
	db	117
	db	95
	db	105
	db	110
	db	116
	db	6
	db	117
	db	95
	db	108
	db	111
	db	110
	db	103
	db	6
	db	83
	db	79
	db	67
	db	75
	db	69
	db	84
	db	13
	db	79
	db	76
	db	69
	db	67
	db	76
	db	73
	db	80
	db	70
	db	79
	db	82
	db	77
	db	65
	db	84
	db	7
	db	72
	db	79
	db	66
	db	74
	db	69
	db	67
	db	84
	db	8
	db	76
	db	72
	db	83
	db	69
	db	82
	db	86
	db	69
	db	82
	db	11
	db	76
	db	72
	db	67
	db	76
	db	73
	db	69
	db	78
	db	84
	db	68
	db	79
	db	67
	db	11
	db	76
	db	72
	db	83
	db	69
	db	82
	db	86
	db	69
	db	82
	db	68
	db	79
	db	67
	db	9
	db	83
	db	67
	db	95
	db	72
	db	65
	db	78
	db	68
	db	76
	db	69
	db	21
	db	83
	db	69
	db	82
	db	86
	db	73
	db	67
	db	69
	db	95
	db	83
	db	84
	db	65
	db	84
	db	85
	db	83
	db	95
	db	72
	db	65
	db	78
	db	68
	db	76
	db	69
	db	7
	db	83
	db	67
	db	95
	db	76
	db	79
	db	67
	db	75
	db	8
	db	108
	db	116
	db	107
	db	95
	db	109
	db	98
	db	108
	db	107
	db	8
	db	108
	db	116
	db	107
	db	95
	db	109
	db	98
	db	108
	db	107
	db	6
	db	108
	db	116
	db	107
	db	115
	db	101
	db	103
	db	6
	db	108
	db	116
	db	107
	db	104
	db	100
	db	108
$$BNAMES	ends
$$BROWSE	segment byte public use32 'DEBSYM'
d8195@	label	byte
$$BROWSE	ends
$$BROWFILE	segment byte public use32 'DEBSYM'
d8196@	label	byte
$$BROWFILE	ends
	end
