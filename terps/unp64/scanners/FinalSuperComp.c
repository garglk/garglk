/* FinalSuperCompressor/flexible */
#include "../unp64.h"
void Scn_FinalSuperComp(unpstr *Unp)
{
    unsigned char *mem;
    int q,p;
    if ( Unp->IdFlag )
        return;
    mem=Unp->mem;
    if( Unp->DepAdr==0 )
    {
        if(((*(unsigned int*)(mem+0x810)&0xff00ff00)==0x9A00A200) &&
            (*(unsigned int*)(mem+0x832)==0x0DF008C9) &&
            (*(unsigned int*)(mem+0x836)==0xB1083DCE) &&
            (*(unsigned int*)(mem+0x8af)==0x4C00FFBD) &&
            ((mem[0x88e]==0x4c)||(mem[0x889]==0x4c)) )
        {
            mem[0x812]=0xff; /* fixed, can't be otherwise */
            Unp->DepAdr=mem[0x856]|mem[0x857]<<8;
            if(Unp->info->run == -1)
                Unp->Forced=0x811;
            for(p=0x889;p<0x88f;p++)
            {
                if(mem[p]==0x4c)
                {
                    Unp->RetAdr=mem[p+1]|mem[p+2]<<8;
                    break;
                }
            }
            Unp->StrMem=mem[0x872]|mem[0x873]<<8;
            Unp->EndAdr=mem[0x84e]|mem[0x852]<<8;Unp->EndAdr++;
            PrintInfo(Unp,_I_SUPCOMFLEX);
            Unp->IdFlag=1;return;
        }
    }
    /* SC/EqualSequences, FinalPack4/UA and TLC packer (sys2061/2064/2065/2066) */
    if( Unp->DepAdr==0 )
    {
        for(p=0x80d;p<0x830;p++)
        {
            if(mem[p+2]==0xb9)
            {
                if( mem[p+5]==0x99 && mem[p+0x15]==0x4c &&
                    mem[p+0x10]==mem[p+0x81] &&
                    (*(unsigned int*)(mem+p+0x78)==0x18F7D0FC) &&
                    (*(unsigned int*)(mem+p+0x8c)==0xD0FCC4C8) )
                {
                    Unp->DepAdr=mem[p+0x16]|mem[p+0x17]<<8;
                    for(q=0x80b;q<0x813;q++)
                    {
                        if(mem[q]==0x78)
                        {
                            Unp->Forced=q;
                            break;
                        }
                    }
                    if(q==0x813)
                    {
                        if(mem[p]==0xa0)
                            Unp->Forced=p;
                    }
                    Unp->EndAdr=mem[p+0x10]; /* either 0x2d or 0xae */
                    Unp->StrMem=mem[p+0x2c]|mem[p+0x2d]<<8; /* $0800 fixed? */
                    p+=0x40;
                    while((mem[p]!=0x4c) && (p<0x860))
                        p++;
                    Unp->RetAdr=mem[p+1]|mem[p+2]<<8;
                    PrintInfo(Unp,_I_SUPCOMEQSE);
                    break;
                }
            }
        }
        if(Unp->DepAdr)
        {
            Unp->IdFlag=1;return;
        }
    }
    /* SC/Equal chars */
    if( Unp->DepAdr==0 )
    {
        if(((*(unsigned int*)(mem+0x810)&0xf0ffffff)==0xA0A93878) &&
           ((*(unsigned int*)(mem+0x814)==0xFC852DE5)||
            (*(unsigned int*)(mem+0x814)==0xFC85aeE5))&&
           ((*(unsigned int*)(mem+0x818)==0xafE508A9)||
            (*(unsigned int*)(mem+0x818)==0x2EE508A9))&&
           ((*(unsigned int*)(mem+0x844)&0x00ffffff)==0x00004C9A) )
        {
            Unp->DepAdr=mem[0x846]|mem[0x847]<<8;
            Unp->Forced=0x810;
            Unp->RetAdr=mem[0x872]|mem[0x873]<<8;
            Unp->EndAdr=mem[0x866]; /* either 0x2d or 0xae */
            Unp->StrMem=mem[0x857]|mem[0x858]<<8;
            Unp->StrMem+=mem[0x849]; /* ldx #$?? -> sta $????,x; not always 0! */
            PrintInfo(Unp,_I_SUPCOMEQCH);
            Unp->IdFlag=1;return;
        }
    }
    /* FinalSuperCompressor/flexible hacked? */
    if( Unp->DepAdr==0 )
    {
        if(((*(unsigned int*)(mem+0x80d)&0xff00ffff)==0x9A00A278) &&
            (*(unsigned int*)(mem+0x814)==0x9D0847BD) &&
            (*(unsigned int*)(mem+0x818)==0x21BD0334) &&
            (*(unsigned int*)(mem+0x81c)==0x040E9D09) &&
            mem[0x878]==0x4c)
        {
            mem[0x80f]=0xff; /* fixed, can't be otherwise */
            Unp->DepAdr=0x334;
            if(Unp->info->run == -1)
                Unp->Forced=0x80d;
            Unp->RetAdr=mem[0x879]|mem[0x87a]<<8;
            //Unp->StrMem=mem[0x861]|mem[0x862]<<8;
            /* patch */
            mem[0x2a7]=0x85;
            mem[0x2a8]=0x01;
            mem[0x2a9]=0x4c;
            mem[0x2aa]=mem[0x879];
            mem[0x2ab]=mem[0x87a];
            Unp->StrMem=0x2a7;
            Unp->fEndAf=0x34e; Unp->EndAdC=0xffff;
            Unp->Recurs=RECUMAX-2;
            PrintInfo(Unp,_I_SUPCOMFH11);
            Unp->IdFlag=1;return;
        }
    }
    /* FinalSuperCompressor/flexible hacked? 2nd layer */
    if( Unp->DepAdr==0 )
    {
        if(Unp->info->start == 0x2a7 )
        {
            if( mem[0x2a7]==0x85&&
                mem[0x2a8]==0x01&&
                mem[0x2a9]==0x4c )
            {
                p=mem[0x2aa]|mem[0x2ab]<<8;
                if( (*(unsigned int*)(mem+p     )==((((mem[0x2aa]+0x15)&0xff)<<24)|0x00B99EA0)) &&
                    (*(unsigned int*)(mem+p+0x96)==0x033B4CFE) )
                {
                    Unp->Forced=0x2a7;
                    Unp->DepAdr=0x334;
                    Unp->RetAdr=mem[p+0x2c]|mem[p+0x2d]<<8;
                    //Unp->StrMem=mem[p+0x17]|mem[p+0x18];
                    Unp->StrMem=0x2ac;
                    mem[0x2ac]=0x85;
                    mem[0x2ad]=0x01;
                    mem[0x2ae]=0x4c;
                    mem[0x2af]=mem[p+0x2c];
                    mem[0x2b0]=mem[p+0x2d];
                    Unp->fEndAf=0x335; Unp->EndAdC=EA_USE_Y|0xffff;
                    PrintInfo(Unp,_I_SUPCOMFH12);
                    Unp->IdFlag=1;return;
                }
            }
        }
    }
    /* FinalSuperCompressor/flexible hacked? 3rd layer */
    if( Unp->DepAdr==0 )
    {
        if(Unp->info->start == 0x2ac )
        {
            if( mem[0x2ac]==0x85&&
                mem[0x2ad]==0x01&&
                mem[0x2ae]==0x4c )
            {
                p=mem[0x2af]|mem[0x2b0]<<8;
                if( (mem[p]==0xa9) &&
                    (*(unsigned int*)(mem+p+0x0a)==0xAF852E85)&&
                    (*(unsigned int*)(mem+p+0x1f)==0x8D034B4C) )
                {
                    Unp->Forced=0x2ac;
                    Unp->DepAdr=0x34b;
                    Unp->fEndAf=0x2d;
                    PrintInfo(Unp,_I_SUPCOMFH13);
                    Unp->IdFlag=1;return;
                }
            }
        }
    }
    /* another special hack */
    if( Unp->DepAdr==0 )
    {
        if(((*(unsigned int*)(mem+0x80d)&0xff00ffff)==0x9a00a278) &&
            (*(unsigned int*)(mem+0x811)==0x018534a9) &&
            (*(unsigned int*)(mem+0x818)==0x34990846) &&
            (*(unsigned int*)(mem+0x81c)==0x0927b903) &&
            (mem[0x87e]==0x4c))
        {
            mem[0x80f]=0xff; /* fixed, can't be otherwise */
            Unp->DepAdr=0x334;
            if(Unp->info->run == -1)
                Unp->Forced=0x80d;
            //Unp->RetAdr=mem[0x87f]|mem[0x880]<<8;
            Unp->RetAdr=0x7f8;
            //Unp->StrMem=mem[0x860]|mem[0x861]<<8;
            Unp->StrMem=Unp->RetAdr;
            Unp->fEndAf=0x34e;
            p=0x7f8;
            mem[p++]=0x85;
            mem[p++]=0x01;
            mem[p++]=0x4c;
            mem[p++]=mem[0x87f];
            mem[p++]=mem[0x880];
            //Unp->Recurs=6;
            PrintInfo(Unp,_I_SUPCOMFH21);
            Unp->IdFlag=1;return;
        }
    }
    /* Flexible / Generic Hack */
    if( Unp->DepAdr==0 )
    {
        if( (*(unsigned int*)(mem+0x8b0)==0x4D4C00FF) &&
            (*(unsigned int*)(mem+0x918)==0x4D4C0187) &&
            (*(unsigned int*)(mem+0x818)==0x57BDCCA2) &&
           ((*(unsigned int*)(mem+0x81c)&0xf0ffffff)==0x00339d08) &&
            (mem[0x88e]==0x4c))
        {
            Unp->DepAdr=mem[0x856]|mem[0x857]<<8;
            for(p=0x80b;p<0x818;p++)
            {
                if(mem[p]==0x78)
                {
                    Unp->Forced=p;
                    break;
                }
            }
            Unp->RetAdr=mem[0x88f]|mem[0x890]<<8;
            Unp->StrMem=mem[0x872]|mem[0x873]<<8;
            Unp->EndAdr=mem[0x84e]|mem[0x852]<<8;Unp->EndAdr++;
            PrintInfo(Unp,_I_SUPCOMHACK);
            Unp->IdFlag=1;return;
        }
    }
    /* eq seq + indirect jmp ($0348) -> expects $00b9 containing $60 (rts) */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x810)==0x8534A978) &&
           (*(unsigned int*)(mem+0x834)==0x03354C48) &&
           (*(unsigned int*)(mem+0x850)==0xF0C81BF0) &&
           (*(unsigned int*)(mem+0x865)==0xEE03486C) )
        {
            Unp->DepAdr=0x335;
            Unp->Forced=0x810;
            Unp->RetAdr=mem[0x82f]|mem[0x80e]<<8;Unp->RetAdr++;
            mem[0xb9]=0x60;
            mem[0x865]=0x4c;
            mem[0x866]=Unp->RetAdr&0xff;
            mem[0x867]=Unp->RetAdr>>8;
            Unp->EndAdr=mem[0x825]; /* either 0x2d or 0xae */
            Unp->StrMem=mem[0x84c]|mem[0x84d]<<8;
            PrintInfo(Unp,_I_SUPCOMEQB9);
            Unp->IdFlag=1;return;
        }
    }
    /* eq char hack, usually 2nd layer of previous eq seq hack */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x810)==0x1EB97FA0) &&
           (*(unsigned int*)(mem+0x814)==0x033B9908) &&
           (*(unsigned int*)(mem+0x838)==0xFE912DB1) &&
           (*(unsigned int*)(mem+0x88a)==0xFE850369) )
        {
            Unp->DepAdr=0x33b;
            Unp->Forced=0x810;
            Unp->RetAdr=mem[0x89a]|mem[0x89b]<<8;
            Unp->fEndAf=0x2d;
            Unp->StrMem=0x800; /* fixed */
            PrintInfo(Unp,_I_SUPCOMEQC9);
            Unp->IdFlag=1;return;
        }
    }
}
