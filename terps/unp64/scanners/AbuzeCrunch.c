/* AbuzeCrunch */
#include "../unp64.h"
void Scn_AbuzeCrunch(unpstr *Unp)
{
    unsigned char *mem;
    int q,p=0;
    if ( Unp->IdFlag )
        return;
    mem=Unp->mem;
    if( Unp->DepAdr==0 )
    {
        for(q=0x80b;q<0x820;q++)
        {
            if((*(unsigned int*)(mem+q     )==0x1BA200A0) &&
               (*(unsigned int*)(mem+q+0x13)==0x10CAA095) &&
               (*(unsigned short int*)(mem+q+0x20)==0x4CF7) )
            {
                p=mem[q+0xc]|mem[q+0xd]<<8;
                if((*(unsigned int*)(mem+p+0x25)==0xFFA9FE91)||
                   (*(unsigned int*)(mem+p+0x25)==0xFeA5FE91) )
                {
                    Unp->DepAdr=mem[q+0x22]|mem[q+0x23]<<8;
                    break;
                }
            }
         }
         if( Unp->DepAdr )
         {
            Unp->EndAdr=mem[p+0x4]|mem[p+0x5]<<8;Unp->EndAdr++;
            Unp->Forced=q;
            if(mem[p+0x2f]==0xa5)
                Unp->RetAdr=mem[p+0x3f]|mem[p+0x40]<<8;
            if(mem[p+0x2f]==0xa9)
                Unp->RetAdr=mem[p+0x3d]|mem[p+0x3e]<<8;
            Unp->fStrAf=0xfe;
            PrintInfo(Unp,_I_ABUZECRUNCH);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        for(q=0x80b;q<0x820;q++)
        {
            if((*(unsigned int*)(mem+q    )==0xA27800A0) &&
               (*(unsigned int*)(mem+q+4  )==0xBAA59AFF) &&
               (*(unsigned short int*)(mem+q+0x2c)==0x4CF7) )
            {
                p=mem[q+0x12]|mem[q+0x13]<<8;
                if( *(unsigned int*)(mem+p+0x25)==0xFeA5FE91 )
                {
                    Unp->DepAdr=mem[q+0x2e]|mem[q+0x2f]<<8;
                    break;
                }
            }
         }
         if( Unp->DepAdr )
         {
            Unp->EndAdr=mem[p+0x4]|mem[p+0x5]<<8;Unp->EndAdr++;
            Unp->Forced=q;
            Unp->RetAdr=mem[p+0x42]|mem[p+0x43]<<8;
            Unp->fStrAf=0xfe;
            PrintInfo(Unp,_I_ABUZECR37);
            Unp->IdFlag=1;return;
        }
    }
    /* Abuze 5.0/FLT */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x80b)==0x1BA200A0) &&
           (*(unsigned int*)(mem+0x813)==0x10CAA095) &&
           (*(unsigned int*)(mem+0x822)==0x011F4C01) )
        {
            p=mem[0x819]|mem[0x81a]<<8;
            if(*(unsigned int*)(mem+p+0x06)==0x9101E520)
            {
                Unp->DepAdr=0x11f;
                if(Unp->info->run == -1)
                    Unp->Forced=0x80b;
                Unp->RetAdr=mem[p+0x23]|mem[p+0x24]<<8;
                Unp->EndAdr=mem[p+0x4]|mem[p+0x5]<<8;Unp->EndAdr++;
                Unp->fStrAf=0xfe;
                PrintInfo(Unp,_I_ABUZECR50);
                Unp->IdFlag=1;return;
            }
        }
    }
}
