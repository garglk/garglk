/* Caution Quickpacker, 3 variants */
#include "../unp64.h"
void Scn_Caution(unpstr *Unp)
{
    unsigned char *mem;

    if ( Unp->IdFlag )
        return;
    mem=Unp->mem;
    /* quickpacker 1.0 sysless */
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x801)==0xE67800A2) &&
            (*(unsigned int*)(mem+0x805)==0x07EDBD01) &&
            (*(unsigned int*)(mem+0x80d)==0x00284CF8) &&
            (*(unsigned int*)(mem+0x844)==0xAC00334C) )
        {
            Unp->Forced=0x801;
            Unp->DepAdr=0x28;
            Unp->RetAdr=mem[0x86b]|mem[0x86c]<<8;
            Unp->EndAdr=mem[0x85a]|mem[0x85b]<<8;
            Unp->fStrAf=mem[0x863];
            Unp->StrAdC=EA_ADDFF|0xffff;
            PrintInfo(Unp,_I_CAUTION10);
            Unp->IdFlag=1;return;
        }
    }
    /* quickpacker 2.x + sys */
    if( Unp->DepAdr==0 )
    {
        if(((*(unsigned int*)(mem+0x80b)&0xf0ffffff)==0x60A200A0) &&
            (*(unsigned int*)(mem+0x80f)==0x0801BD78) &&
            (*(unsigned int*)(mem+0x813)==0xD0CA0095) &&
            (*(unsigned int*)(mem+0x81e)==0xD0C80291) &&
            (*(unsigned int*)(mem+0x817)==0x001A4CF8) )
        {
            Unp->Forced=0x80b;
            Unp->DepAdr=0x01a;
            if(mem[0x80e]==0x69)
            {
                Unp->RetAdr=mem[0x842]|mem[0x843]<<8;
                Unp->EndAdr=mem[0x850]|mem[0x851]<<8;Unp->EndAdr+=0x100;
                Unp->fStrAf=0x4f;
                Unp->StrAdC=0xffff|EA_USE_Y;
                PrintInfo(Unp,_I_CAUTION25);
                Unp->IdFlag=1;return;
            }
            else if(mem[0x80e]==0x6c)
            {
                Unp->RetAdr=mem[0x844]|mem[0x845]<<8;
                Unp->EndAdr=mem[0x84e]|mem[0x84f]<<8;Unp->EndAdr++;
                Unp->fStrAf=0x4d;
                PrintInfo(Unp,_I_CAUTION20);
                Unp->IdFlag=1;return;
            }
        }
    }
    /* strangely enough, sysless v2.0 depacker is at $0002 */
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x83d)==0xAA004A20) &&
            (*(unsigned int*)(mem+0x801)==0xA27800A0) &&
            (*(unsigned int*)(mem+0x805)==0x080FBD55) &&
            (*(unsigned int*)(mem+0x809)==0xD0CA0095) &&
            (*(unsigned int*)(mem+0x80d)==0x00024CF8) )
        {
            Unp->Forced=0x801;
            Unp->DepAdr=0x2;
            Unp->RetAdr=mem[0x83b]|mem[0x83c]<<8;
            Unp->EndAdr=mem[0x845]|mem[0x846]<<8;Unp->EndAdr++;
            Unp->fStrAf=mem[0x849];
            //Unp->StrAdC=0xffff;
            PrintInfo(Unp,_I_CAUTION20SS);
            Unp->IdFlag=1;return;
        }
    }
    /* same goes for v2.5 sysless, seems almost another packer */
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x83b)==0xAA005520) &&
            (*(unsigned int*)(mem+0x801)==0x60A200A0) &&
            (*(unsigned int*)(mem+0x805)==0x0801BD78) &&
            (*(unsigned int*)(mem+0x809)==0xD0CA0095) &&
            (*(unsigned int*)(mem+0x80d)==0x00104CF8) )
        {
            Unp->Forced=0x801;
            Unp->DepAdr=0x10;
            Unp->RetAdr=mem[0x839]|mem[0x83a]<<8;
            Unp->EndAdr=mem[0x847]|mem[0x848]<<8;Unp->EndAdr+=0x100;
            Unp->fStrAf=0x46;
            Unp->StrAdC=0xffff|EA_USE_Y;
            PrintInfo(Unp,_I_CAUTION25SS);
            Unp->IdFlag=1;return;
        }
    }
    /* hardpacker */
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x80d)==0x8534A978) &&
            (*(unsigned int*)(mem+0x811)==0xB9B3A001) &&
            (*(unsigned int*)(mem+0x815)==0x4C99081F) &&
            (*(unsigned int*)(mem+0x819)==0xF7D08803) &&
            (*(unsigned int*)(mem+0x81d)==0xB9034D4C) )
        {
            Unp->Forced=0x80d;
            Unp->DepAdr=0x34d;
            Unp->RetAdr=mem[0x87f]|mem[0x880]<<8;
            Unp->EndAdr=mem[0x88d]|mem[0x88e]<<8;
            Unp->fStrAf=0x3ba;
            Unp->StrAdC=EA_ADDFF|0xffff;
            PrintInfo(Unp,_I_CAUTIONHP);
            Unp->IdFlag=1;return;
        }
    }
}
