/* CCS packers */
#include "../unp64.h"
void Scn_CCS(unpstr *Unp)
{
    unsigned char *mem;
    int p;
    if ( Unp->IdFlag )
        return;
    mem=Unp->mem;
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x817)==0xB901E678) &&
            (*(unsigned int*)(mem+0x81b)==0xFD990831) &&
            (*(unsigned int*)(mem+0x8ff)==0xFEE60290) &&
            (*(unsigned int*)(mem+0x90f)==0x02903985)
          )
        {
            if(Unp->info->run == -1)
                Unp->Forced=0x817;
            Unp->DepAdr=0x0ff;
            Unp->fEndAf=0x2d;
            Unp->EndAdC=0xffff;
            Unp->RetAdr=mem[0x8ed]|mem[0x8ee]<<8;
            if(Unp->RetAdr==0xa659)
            {
                mem[0x8ec]=0x2c;
                Unp->RetAdr=mem[0x8f0]|mem[0x8f1]<<8;
            }
            PrintInfo(Unp,_I_CCSMAXS);
            Unp->IdFlag=1;return;
        }
    }
    /* derived from supercomp/eqseq */
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x80b)==0x8C7800A0) &&
            (*(unsigned int*)(mem+0x812)==0x0099082F) &&
            (*(unsigned int*)(mem+0x846)==0x0DADF2D0) &&
            (*(unsigned int*)(mem+0x8c0)==0xF001124C)
          )
        {
            if(Unp->info->run == -1)
                Unp->Forced=0x80b;
            Unp->DepAdr=0x100;
            Unp->EndAdr=0xae;
            Unp->RetAdr=mem[0x8f1]|mem[0x8f2]<<8;
            if(Unp->RetAdr==0xa659)
            {
                mem[0x8f0]=0x2c;
                Unp->RetAdr=mem[0x8f4]|mem[0x8f5]<<8;
            }
            PrintInfo(Unp,_I_SUPCOMEQCCS);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x814)==0xB901E678) &&
            (*(unsigned int*)(mem+0x818)==0xFD990829) &&
            (*(unsigned int*)(mem+0x8a1)==0xFDA6FDB1) &&
            (*(unsigned int*)(mem+0x8a5)==0xFEC602D0)
          )
        {
            if(Unp->info->run == -1)
                Unp->Forced=0x814;
            Unp->DepAdr=0x0ff;
            Unp->fEndBf=0x39;
            PrintInfo(Unp,_I_CCSPACK);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x818)==0x2CB901E6) &&
            (*(unsigned int*)(mem+0x81c)==0x00FB9908) &&
            (*(unsigned int*)(mem+0x850)==0xFBB1C84A) &&
            (*(unsigned int*)(mem+0x854)==0xB1C81185)
          )
        {
            if(Unp->info->run == -1)
                Unp->Forced=0x812;
            Unp->DepAdr=0x0ff;
            Unp->EndAdr=0xae;
            PrintInfo(Unp,_I_CCSCRUNCH1);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x818)==0x2CB901E6) &&
            (*(unsigned int*)(mem+0x81c)==0x00FB9908) &&
            (*(unsigned int*)(mem+0x851)==0xFBB1C812) &&
            (*(unsigned int*)(mem+0x855)==0xB1C81185)
          )
        {
            if(Unp->info->run == -1)
                Unp->Forced=0x812;
            Unp->DepAdr=0x0ff;
            Unp->EndAdr=0xae;
            PrintInfo(Unp,_I_CCSCRUNCH2);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x82c)==0x018538A9) &&
            (*(unsigned int*)(mem+0x831)==0xFD990842) &&
            (*(unsigned int*)(mem+0x83e)==0x00FF4CF1) &&
            (*(unsigned int*)(mem+0x8a5)==0x50C651C6)
          )
        {
            if(Unp->info->run == -1)
                Unp->Forced=0x822;
            Unp->DepAdr=0x0ff;
            Unp->fEndBf=0x39;
            Unp->RetAdr=mem[0x8ea]|mem[0x8eb]<<8;
            PrintInfo(Unp,_I_CCSSCRE);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned short int*)(mem+0x81a)==0x00A0) &&
           ((*(unsigned int*)(mem+0x820)==0xFB990837)||
            (*(unsigned int*)(mem+0x824)==0xFB990837) ) &&
            (*(unsigned int*)(mem+0x83b)==0xFD91FBB1) &&
            (*(unsigned int*)(mem+0x8bc)==0xEE00FC99)
          )
        {
            if(Unp->info->run == -1)
                Unp->Forced=0x81a;
            Unp->DepAdr=0x0ff;
            Unp->fEndAf=0x39;
            Unp->EndAdC=0xffff;
            Unp->RetAdr=mem[0x8b3]|mem[0x8b4]<<8;
            PrintInfo(Unp,_I_CCSSPEC);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x812)==0xE67800A0) &&
            (*(unsigned int*)(mem+0x816)==0x0823B901) &&
            (*(unsigned int*)(mem+0x81a)==0xC800FD99) &&
            (*(unsigned int*)(mem+0x81e)==0xFF4CF7D0) &&
            (*(unsigned int*)(mem+0x885)==0xFDA6FDB1)
          )
        {
            if(Unp->info->run == -1)
                Unp->Forced=0x812;
            Unp->DepAdr=0x0ff;
            // $2d is unreliable, Executer uses line number at $0803/4,
            // which is read at $0039/3a by basic, as end address,
            // then can set arbitrarily $2d/$ae pointers after unpack.
            //Unp->fEndAf=0x2d;
            Unp->EndAdr=mem[0x803]|mem[0x804]<<8;
            Unp->EndAdr++;
            if(*(unsigned int*)(mem+0x87f)==0x4CA65920)
                mem[0x87f]=0x2c;
            Unp->RetAdr=mem[0x883]|mem[0x884]<<8;
            PrintInfo(Unp,_I_CCSEXEC);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x812)==0xE67800A0) &&
            (*(unsigned int*)(mem+0x816)==0x084CB901) &&
            (*(unsigned int*)(mem+0x81a)==0xA900FB99) &&
            (*(unsigned int*)(mem+0x848)==0x00FF4CE2)
          )
        {
            if(Unp->info->run == -1)
                Unp->Forced=0x812;
            Unp->DepAdr=0x0ff;
            Unp->fEndAf=0x2d;
            Unp->EndAdC=0xffff;
            PrintInfo(Unp,_I_CCSUNKN);
            Unp->IdFlag=1;return;
        }
    }
    /* Triad Hack */
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x838)==0xB9080099) &&
            (*(unsigned int*)(mem+0x83f)==0xD0880816) &&
            (*(unsigned int*)(mem+0x8ff)==0xFEE60290) &&
            (*(unsigned int*)(mem+0x90f)==0x02903985)
          )
        {
            if(Unp->info->run == -1)
            {
                for(p=0x80b;p<0x820;p++)
                {
                    if((mem[p]&0xa0)==0xa0)
                    {
                        Unp->Forced=p;
                        break;
                    }
                }
            }
            Unp->DepAdr=0x0ff;
            Unp->fEndAf=0x2d;
            Unp->EndAdC=0xffff;
            Unp->RetAdr=mem[0x8ed]|mem[0x8ee]<<8;
            if(Unp->RetAdr==0xa659)
            {
                mem[0x8ec]=0x2c;
                Unp->RetAdr=mem[0x8f0]|mem[0x8f1]<<8;
            }
            PrintInfo(Unp,_I_CCSHACK);
            Unp->IdFlag=1;return;
        }
    }
}
