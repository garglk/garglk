/*
 *   Former implementations of os_exfld, os_exfil, and os_excall.  All
 *   external function operations have been removed as of 2.5.8 - external
 *   functions are now obsolete due to security concerns.  
 */

#ifdef DJGPP

/*
 *   os_exfld - load in an external function from an open file, given the
 *   size of the function (in bytes).  Returns a pointer to the newly
 *   allocated memory block containing the function in memory.  
 */
int (*os_exfld( osfildef *fp, unsigned len ))(void *)
{
    return  (int (*)(void *)) 0;
}

/*
 *   Load an external function from a file.  This routine assumes that the
 *   file has the same name as the resource.  
 */
int (*os_exfil(const char *name))(void *)
{
    return (int (*)(void *)) 0;
}

/*
 *   call an external function, passing it an argument (a string pointer),
 *   and passing back the string pointer returned by the external function 
 */
int os_excall(int (*extfn)(void *), void *arg)
{
    return 0;
}

#endif /* DJGPP */

/* ------------------------------------------------------------------------ */
/*
 *   Real-mode and 16-bit protected mode external function interfaces 
 */

#ifndef T_WIN32

static void *canon(void *ptr)
{
    unsigned long p = (unsigned long)ptr;
    unsigned long abs = ((p >> 16) << 4) + (p & 0xffff);
    if (abs & (unsigned long)0xf)
        abs = (abs & ~(unsigned long)0xf) + (unsigned long)0x10;
    return((void *)(abs << 12));
}

/*
 *   os_exfld - load in an external function from an open file, given the
 *   size of the function (in bytes).  Returns a pointer to the newly
 *   allocated memory block containing the function in memory.  
 */
int (*os_exfld(osfildef *fp, unsigned len))(void *)
{
    void      *extfn;
    unsigned   alo;

#ifdef MSOS2
    /* for OS/2, don't load anything, but seek past the resource */
# ifdef MICROSOFT
    if (_osmode == OS2_MODE)
# endif /* MICROSOFT */
    {
        osfseek(fp, (long)len, OSFSK_SET);
        return((int (*)(void))0);
    }
#endif /* MSOS2 */

#ifdef __DPMI16__

    HANDLE selector;

    selector = GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT, len);
    if (!selector || !(extfn = GlobalLock(selector)))
        return 0;

    /* read the file */
    osfrb(fp, extfn, len);

    /* change the selector to a code segment */
    _asm
    {
        mov   bx, selector                                  /* get selector */
            lar   ax, bx                       /* get current access rights */
            mov   cl, ah
            or    cl, 8               /* set the CODE bit in the descriptor */
            mov   ax, 9                 /* function = set descriptor rights */
            mov   ch, 0
            int   31h
    }

    /* close the file and return the pointer to the function in memory */
    return((int (*)(void *))extfn);

#else /* __DPMI16 __ */

    /* figure out how much memory is needed and allocate it */
    alo = ((len + 0xf) & ~0xf) + 16;    /* round to mult of 16, plus 1 page */
    extfn = canon(malloc(alo));/* allocate the memory, canonicalize pointer */
    if (!extfn)
        return((int (*)(void *))0 );

    /* read the file */
    osfrb(fp, extfn, len);

    /* close the file and return the pointer to the function in memory */
    return((int (*)(void *))extfn);

#endif /* __DPMI16__ */
}

/*
 *   Load an external function from a file.  This routine assumes that the
 *   file has the same name as the resource.  
 */
int (*os_exfil(const char *name))(void *)
{
    FILE      *fp;
    unsigned   len;
    int      (*extfn)(void *);

#ifdef MSOS2
    /* load the function from a DLL of the same name as the function */
# ifdef MICROSOFT
    if (_osmode == OS2_MODE)
# endif /* MICROSOFT */
    {
        CHAR    failname[128];
        HMODULE hmod;
        PFN     pfn;

        if (DosLoadModule(failname, sizeof(failname), (PSZ)name, &hmod)
            || DosGetProcAddr(hmod, (PSZ)"_main", &pfn))
        {
            return((int (*)(void))0);
        }
        return((int (*)(void))pfn);
    }
#endif /* MSOS2 */

#ifdef __DPMI16__

#endif

    /* open the file and see how big it is to determine our memory needs */
    if ( !( fp = fopen( name, "rb" ))) return( (int (*)(void *))0 );
    (void)fseek( fp, 0L, 2 );
    len = (unsigned)ftell( fp );                    /* total length of file */

    (void)fseek( fp, 0L, 0 );
    extfn = os_exfld(fp, len);

    fclose( fp );
    return( extfn );
}

/*
 *   call an external function, passing it an argument (a string pointer),
 *   and passing back the string pointer returned by the external function 
 */
int os_excall(int (*extfn)(void *), void *arg)
{
    return((*extfn)(arg));
}

#endif /* !T_WIN32 */

/* ------------------------------------------------------------------------ */
/*
 *   Win32 external function interfaces - not implemented at present 
 */

#ifdef T_WIN32

int (*os_exfld(osfildef *fp, unsigned len))(void *)
{
    /* NOT IMPLEMENTED - scan past the resource and fail */
    osfseek(fp, (long)len, OSFSK_CUR);
    return 0;
}

int (*os_exfil(const char *name))(void *)
{
    HINSTANCE hlib;
    FARPROC proc;

    /* 
     *   load the library of the given name; if we can't load the library, we
     *   can't load the external function 
     */
    hlib = LoadLibrary(name);
    if (hlib == 0)
        return 0;

    /* get the address of the "_main" procedure */
    proc = GetProcAddress(hlib, "main");

    /* if that failed, unload the library and return failure */
    if (proc == 0)
    {
        FreeLibrary(hlib);
        return 0;
    }

    /* 
     *   return the procedure address, suitably cast; unfortunately, we have
     *   no provision for freeing the library instance handle, so we'll just
     *   have to count on Windows releasing it when the process terminates 
     */
    return (int (*)(void *))proc;
}

int os_excall(int (*extfn)(void *), void *arg)
{
    /* call the function directly */
    return((*extfn)(arg));
}

#endif /* T_WIN32 */

