/* Mr.Cross linker */
#include "../unp64.h"
void Scn_MrCross(unpstr *Unp)
{
    unsigned char *mem;
    int q,p;
    if ( Unp->IdFlag )
        return;
    mem=Unp->mem;
    if( Unp->DepAdr==0 )
    {
        /*
        there are sysless versions and also can be at
        different address than $0801
        */
        p=Unp->info->run;
        if(p==-1)
            p=Unp->info->start;
        for (q=p+0x80;p<q;p++)
        {
            if((*(unsigned int*)(mem+p+0x00)==0x8538A978) &&
               (*(unsigned int*)(mem+p+0x04)==0x9AF9A201) &&
               (*(unsigned int*)(mem+p+0x0b)==0xCA01069D) &&
               (*(unsigned int*)(mem+p+0x36)==0x60D8d000+(p>>8))  /*((p+0x3b)>>8)*/
              )
            {
                Unp->DepAdr=mem[p+0x96]|mem[p+0x97]<<8;Unp->DepAdr+=0x1f;
                Unp->Forced=p;
                Unp->RetAdr=mem[p+0x9a]|mem[p+0x9b]<<8;
                Unp->StrMem=mem[p+0x4d]|mem[p+0x4e]<<8;
                Unp->MonEnd=(mem[p+0x87])<<24|
                            (mem[p+0x86])<<16|
                            (mem[p+0x8d])<< 8|
                            (mem[p+0x8c]);
                PrintInfo(Unp,_I_MRCROSS2);
                break;
            }
            if((*(unsigned int*)(mem+p+0x00)==0x8538A978) &&
               (*(unsigned int*)(mem+p+0x04)==0x9AF9A201) &&
               (*(unsigned int*)(mem+p+0x0b)==0xCA01089D) &&
               (*(unsigned int*)(mem+p+0x36)==0xe0D8d000+(p>>8))  /*((p+0xc8)>>8)*/
              )
            {
                Unp->DepAdr=0x1f6;
                Unp->Forced=p;
                Unp->RetAdr=mem[p+0xaa]|mem[p+0xab]<<8;
                Unp->StrMem=mem[p+0x53]|mem[p+0x54]<<8;
                Unp->fEndAf=0x1a7;
                Unp->EndAdC=0xffff;
                PrintInfo(Unp,_I_MRCROSS1);
                break;
            }
        }
        if(Unp->DepAdr)
        {
            Unp->IdFlag=1;return;
        }
    }
}
