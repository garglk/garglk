/*1001 / TCI*/
#include "../unp64.h"
void Scn_1001card(unpstr *Unp)
{
    unsigned char *mem;
    int q,p;
    if ( Unp->IdFlag )
        return;
    mem=Unp->mem;
    if( Unp->DepAdr==0 )
    {
        if(/*(*(unsigned int*)(mem+0x812)==0x8630A278)*/ mem[0x815]==0x86 &&
           ((*(unsigned int*)(mem+0x816)&0xfff0ffff)==0xB9A0A001) &&
           (*(unsigned int*)(mem+0x81a)==0xFA99082A) &&
           (*(unsigned int*)(mem+0x886)==0x01154CC1) )
        {
            Unp->DepAdr=0x100;
            if(Unp->info->run == -1)
                Unp->Forced=0x815;
            Unp->RetAdr=mem[0x940]|mem[0x941]<<8;
            Unp->EndAdr=mem[0x82e]|mem[0x82f]<<8;
            PrintInfo(Unp,_I_1001_4);
            Unp->IdFlag=1;return;
        }
    }
    /* HTL hack? */
    if( Unp->DepAdr==0 )
    {
        if(mem[0x813]==0xe6 &&
           ((*(unsigned int*)(mem+0x814)&0xfff0ffff)==0xB9A0A001) &&
           (*(unsigned int*)(mem+0x818)==0xee04082A) &&
           (*(unsigned int*)(mem+0x81c)==0xB900FA99) &&
           (*(unsigned int*)(mem+0x886)==0x01154CC1) )
        {
            Unp->DepAdr=0x100;
            if(Unp->info->run == -1)
                Unp->Forced=0x813;
            Unp->RetAdr=mem[0x940]|mem[0x941]<<8;
            Unp->EndAdr=mem[0x82e]|mem[0x82f]<<8;
            PrintInfo(Unp,_I_1001_HTL);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if(/*(*(unsigned int*)(mem+0x812)==0xE6B0A278) &&*/
           (*(unsigned int*)(mem+0x852)==0x0A006D4C) &&
           (*(unsigned int*)(mem+0x816)==0x9D01B501) &&
           (*(unsigned int*)(mem+0x81a)==0x54BD034F) )
        {
            Unp->DepAdr=0x334;
            if(Unp->info->run == -1)
                Unp->Forced=0x812;
            Unp->RetAdr=mem[0x916]|mem[0x917]<<8;
            Unp->fEndBf=0x63;Unp->EndAdC=0xffff;
            PrintInfo(Unp,_I_1001_NEWPACK);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        /*  original is at $0840, there are also hacks at $0801 and others
        */
        for(p=0x0801;p<0x841;p++)
        {
            if(((mem[p]==0x78)||
                (*(unsigned int*)(mem+p)==0x018536A9)||
                (*(unsigned int*)(mem+p)==0x7800A000)
               ) &&
               (mem[p+6]==0xb9)&&
               (*(unsigned short int*)(mem+p+0x7)==(p+0x18)) &&
               (*(unsigned int*)(mem+p+0x09)==0xb900fb99) &&
               (*(unsigned int*)(mem+p+0x0f)==0xC8040099) &&
               (*(unsigned int*)(mem+p+0x13)==0xFF4CF1D0) )
            {
                Unp->DepAdr=0x0ff;
                break;
            }
        }
        if(Unp->DepAdr)
        {
            Unp->Forced=p;
            if(*(unsigned int*)(mem+p)==0x7800A000)
                mem[p]=0xea;
            //Unp->EndAdr=0x2d;
            Unp->EndAdr=mem[p+0x18]|mem[p+0x19]<<8;Unp->EndAdr++;
            for(q=p+0x97;q<p+0xba;q++)
            {
                if((mem[q]==0xa9)||(mem[q]==0x85))
                {
                    q++;
                    continue;
                }
                if((mem[q]==0x20&&mem[q+3]==0x4c)||(mem[q]==0x4c))
                {
                    Unp->RetAdr=mem[q+1]|mem[q+2]<<8;
                    if((mem[q]==0x20)&&(Unp->RetAdr==0xa659))
                    {
                        Unp->RetAdr=mem[q+4]|mem[q+5]<<8;
                        mem[q]=0x2c;
                    }
                    break;
                }
            }
            PrintInfo(Unp,_I_1001_OLDPACK);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x814)==0xA6A00185) &&
           (*(unsigned int*)(mem+0x819)==0xFA990835) &&
           (*(unsigned int*)(mem+0x821)==0x88033399) &&
           (*(unsigned int*)(mem+0x911)==0xA5034D4C) )
        {
            Unp->DepAdr=0x100;
            if(Unp->info->run == -1)
            {
                for(p=0x0801;p<0x813;p++)
                {
                    if(mem[p]== 0x78)
                    {
                        Unp->Forced=p;
                        break;
                    }
                }
            }
            Unp->RetAdr=mem[0x952]|mem[0x953]<<8;
            Unp->EndAdr=mem[0x839]|mem[0x83a]<<8;
            PrintInfo(Unp,_I_1001_CRAM);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x80d)==0x86F0A278) &&
           (*(unsigned int*)(mem+0x81a)==0x01004CF7) &&
           (*(unsigned int*)(mem+0x8fc)==0x017A4CF7) )
        {
            Unp->DepAdr=0x100;
            Unp->Forced=0x80d;
            Unp->RetAdr=mem[0x83f]|mem[0x840]<<8;
            Unp->EndAdr=mem[0x81e]|mem[0x81f]<<8;
            Unp->fStrAf=0xf9;Unp->StrAdC=0xffff;
            PrintInfo(Unp,_I_1001_ACM);
            Unp->IdFlag=1;return;
        }
    }
    /* Datel UltraCompander - variant at 2061, identical to v4/crunch */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x813)==0xCFB95AA0) &&
           (*(unsigned int*)(mem+0x81b)==0x990875B9) &&
           (*(unsigned int*)(mem+0x839)==0xF1D0C808) &&
           (*(unsigned int*)(mem+0x884)==0x01154CC1) )
        {
            Unp->DepAdr=0x100;
            if(Unp->info->run == -1)
                Unp->Forced=0x813;
            for(q=0x92c;q<0x935;q++)
            {
                if(mem[q]==0xfc&&mem[q+9]==0x4c)
                {
                    Unp->RetAdr=mem[q+0xa]|mem[q+0xb]<<8;
                    break;
                }
            }
            Unp->fStrAf=0xfe;Unp->StrAdC=0xffff;
            Unp->EndAdr=mem[0x82c]|mem[0x82d]<<8;
            PrintInfo(Unp,_I_DATELUC);
            Unp->IdFlag=1;return;
        }

    }
    /* Datel UltraCompander - Compactor, very similar to screencrunch */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x80d)==0x1CBD7FA2) &&
           (*(unsigned int*)(mem+0x811)==0x01009D08) &&
           (*(unsigned int*)(mem+0x874)==0x0176200C) &&
           (*(unsigned int*)(mem+0x881)==0xDDD001A2) )
        {
            Unp->Forced=0x80d;
            Unp->DepAdr=0x100;
            Unp->EndAdr=0xae;
            Unp->StrMem=mem[0x84e]|mem[0x850]<<8;
            Unp->RetAdr=mem[0x890]|mem[0x891]<<8;
            PrintInfo(Unp,_I_DATELUCC);
            Unp->IdFlag=1;return;
        }
    }
    /* Datel UltraCompander - Packer */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x80d)==0xA201E678) &&
           (*(unsigned int*)(mem+0x81f)==0x9D0911BD) &&
           (*(unsigned int*)(mem+0x84b)==0x80006D4C) &&
           (*(unsigned int*)(mem+0x90d)==0x02A84C00) )
        {
            Unp->DepAdr=0x2a8;
            if(Unp->info->run == -1)
                Unp->Forced=0x80d;
            Unp->RetAdr=mem[0x85f]|mem[0x860]<<8;
            Unp->fEndBf=0x63;Unp->EndAdC=0xffff;
            PrintInfo(Unp,_I_DATELUCP);
            Unp->IdFlag=1;return;
        }
    }
}
