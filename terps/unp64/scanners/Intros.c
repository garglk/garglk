/* Bypass known intros that need some fiddling */
/* Also some virus added here, they can be easily removed, so why not? */

#include "../unp64.h"
void Scn_Intros(unpstr *Unp)
{
    unsigned char *mem;
    int q=0,p;
    if ( Unp->IdFlag )
        return;
    mem=Unp->mem;
    /* HIV virus */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x80b)==0x10DD002C) &&
           (*(unsigned int*)(mem+0x84f)==0x34A9EDDD) &&
           (*(unsigned int*)(mem+0x8a1)==0xA94D2D57) &&
           (*(unsigned int*)(mem+0x9bc)==0xF004B120) )
        {
            Unp->Forced=0x859;
            Unp->DepAdr=0x100;
            Unp->RetAdr=0xa7ae;
            //Unp->StrMem=mem[0x9fb]|mem[0x9fc]<<8;
            Unp->StrMem=0x801;
            Unp->EndAdr=0x2d;
            PrintInfo(Unp,_I_HIVIRUS);
            Unp->IdFlag=1;return;
        }
    }
    /* BHP virus */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x821)==0xADA707F0) &&
           (*(unsigned int*)(mem+0x825)==0x32A55DA6) &&
           (*(unsigned int*)(mem+0x920)==0xD13D20D1) &&
           (*(unsigned int*)(mem+0xef5)==0x02CD4C20) )
        {
            Unp->Forced=0x831;
            Unp->DepAdr=0x2ab;
            Unp->RetAdr=0xa7ae;
            Unp->RtAFrc=1;
            Unp->StrMem=0x801;
            Unp->EndAdr=0x2d;
            PrintInfo(Unp,_I_BHPVIRUS);
            Unp->IdFlag=1;return;
        }
    }
    /* Bula virus */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x810)==0x78084220) &&
           (*(unsigned int*)(mem+0x853)==0x4DA908F6) &&
           (*(unsigned int*)(mem+0xa00)==0xF00348AD) &&
           (*(unsigned int*)(mem+0xac4)==0x04B14C0D) )
        {
            Unp->Forced=0x810;
            Unp->DepAdr=0x340;
            Unp->RetAdr=0xa7ae;
            Unp->RtAFrc=1;
            Unp->StrMem=0x801;
            Unp->EndAdr=Unp->info->end-0x2fa;
            PrintInfo(Unp,_I_BULAVIRUS);
            Unp->IdFlag=1;return;
        }
    }
    /* Coder virus */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x81f)==0xD3018D08) &&
           (*(unsigned int*)(mem+0x823)==0xA9082D8D) &&
           (*(unsigned int*)(mem+0x89F)==0x78802C20) &&
           (*(unsigned int*)(mem+0x8AF)==0x6CE3974C) )
        {
            Unp->Forced=0x812;
            Unp->DepAdr=0x2b3;
            Unp->RetAdr=0xa7ae;
            Unp->RtAFrc=1;
            Unp->StrMem=0x801;
            Unp->EndAdr=0x2d;
            PrintInfo(Unp,_I_CODERVIRUS);
            Unp->IdFlag=1;return;
        }
    }
    /* Boa virus */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x80b)==0x10A900A0) &&
           (*(unsigned int*)(mem+0x81b)==0xEDDD2008) &&
           (*(unsigned int*)(mem+0x85F)==0x0401004C) &&
           (*(unsigned int*)(mem+0x9F6)==0x04EE4C00) )
        {
            Unp->Forced=0x80b;
            Unp->DepAdr=0x100;
            Unp->RetAdr=0xa7ae;
            Unp->RtAFrc=1;
            Unp->StrMem=0x801;
            Unp->EndAdr=0x2d;
            PrintInfo(Unp,_I_BOAVIRUS);
            Unp->IdFlag=1;return;
        }
    }
    /* Relax intros */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0xB50)==0x0C632078) &&
           (*(unsigned int*)(mem+0xb54)==0xA90C5A20) &&
           (*(unsigned int*)(mem+0xc30)==0x4C0D6420) &&
           (*(unsigned int*)(mem+0xc34)==0x00A2EA31) )
        {
            Unp->Forced=2070;
            Unp->DepAdr=0x100;
            Unp->RetAdr=mem[0xbea]|mem[0xbeb]<<8;
            Unp->RtAFrc=1;
            Unp->StrMem=0x0801;
            Unp->EndAdr=0x2d;
            mem[0xbae]=0x24; /* lda $c5 is not simulated in UNP64 */
            PrintInfo(Unp,_I_IRELAX1);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x816)==0x2ABD00A2) &&
           (*(unsigned int*)(mem+0x81a)==0xCE009D08) &&
           (*(unsigned int*)(mem+0x826)==0xCE004CF1) &&
           (*(unsigned int*)(mem+0x9F9)==0x5A5A5A5A) )
        {
            Unp->Forced=0x816;
            Unp->DepAdr=0xcf7e; /* highmem unpacker always need to be managed */
            Unp->RetAdr=mem[0x9a9]|mem[0x9aa]<<8;
            Unp->RtAFrc=1;
            Unp->StrMem=0x0801;
            Unp->EndAdr=0x2d;
            PrintInfo(Unp,_I_IRELAX2);
            Unp->IdFlag=1;return;
        }
    }
    /* F4CG */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x812)==0x20FDA320) &&
           (*(unsigned int*)(mem+0x890)==0x06309DD8) &&
           (*(unsigned int*)(mem+0x9BF)==0xA2A9D023) &&
           (*(unsigned int*)(mem+0xa29)==0xFFFF2CFF) )
        {
            Unp->Forced=0x812;
            Unp->DepAdr=0x110;
            p=mem[0x8ee];
            Unp->RetAdr=(mem[0xe44]^p)|(mem[0xe45]^p)<<8;
            Unp->StrMem=0x0801;
            Unp->EndAdr=mem[0x8f8]|mem[0x8fc]<<8;
            mem[0xa2d]=0x4c;  /* indirect jmp $0110 using basic rom */
            mem[0xa2e]=0x10;
            mem[0xa2f]=0x01;
            PrintInfo(Unp,_I_IF4CG23);
            Unp->IdFlag=1;return;
        }
    }
    /* Triad */
    if( Unp->DepAdr==0 )
    {
        for(p=0x80d;p<0x828;p++)
        {
            if( (*(unsigned int*)(mem+p+0x000)==0xA9D0228D) &&
                (*(unsigned int*)(mem+p+0x00c)==0x06979D0A) &&
                (*(unsigned int*)(mem+p+0x0a0)==0x03489D03) &&
                (*(unsigned int*)(mem+p+0x0a4)==0x9D0371BD) )
            {

                for(q=0;q<5;q+=4)
                {
                    if( (*(unsigned int*)(mem+p+0x1ec+q)==0x04409D04) &&
                        (*(unsigned int*)(mem+p+0x210+q)==0x607EFF60) )
                    {
                        Unp->DepAdr=0x100;
                        break;
                    }
                }
                break;
            }
        }
        if(Unp->DepAdr)
        {
            Unp->Forced=p;
            Unp->RetAdr=mem[p+q+0x28a]|mem[p+q+1+0x28a]<<8;
            Unp->EndAdr=mem[p+q+0x19e]|mem[p+q+0x1a4]<<8;
            Unp->StrMem=0x801;
            PrintInfo(Unp,_I_ITRIAD1);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x81f)==0xA2D0228D) &&
           (*(unsigned int*)(mem+0x823)==0x0A5CBD28) &&
           (*(unsigned int*)(mem+0x8d7)==0x8509754C) &&
           (*(unsigned int*)(mem+0x975)==0x07A053A2) )
        {
            Unp->DepAdr=0x100;
            Unp->Forced=0x81f;
            Unp->RetAdr=mem[0xac3]|mem[0xac4]<<8;
            Unp->EndAdr=mem[0x9cd]|mem[0x9d3]<<8;
            Unp->StrMem=0x801;
            PrintInfo(Unp,_I_ITRIAD2);
            Unp->IdFlag=1;return;
        }
    }
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x821)==0xA9D0228D) &&
           (*(unsigned int*)(mem+0x82a)==0x0A3aBD28) &&
           (*(unsigned int*)(mem+0x8a7)==0x85093c4C) &&
           (*(unsigned int*)(mem+0x93c)==0x17108EC6) )
        {
            Unp->DepAdr=0x100;
            Unp->Forced=0x821;
            Unp->RetAdr=mem[0xa99]|mem[0xa9a]<<8;
            Unp->EndAdr=mem[0x9bc]|mem[0x9c2]<<8;
            Unp->StrMem=0x801;
            PrintInfo(Unp,_I_ITRIAD5);
            Unp->IdFlag=1;return;
        }
    }
    /* Snacky/G*P */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x818)==0x018533A9) &&
           (*(unsigned int*)(mem+0x888)==0x08684C03) &&
           (*(unsigned int*)(mem+0x898)==0x40A9FAD0) &&
           (*(unsigned int*)(mem+0xa2d)==0x0A2E4C58) )
        {
            for(q=0xc3c;q<0xc4f;q++)
            {
                if((mem[q  ]==0xa9)&&
                   (mem[q+2]==0x85)&&
                   (mem[q+3]==0x01)&&
                   (mem[q+4]==0x4c))
                {
                    Unp->Forced=q;
                    Unp->EndAdr=mem[q+5]|mem[q+6]<<8;
                    break;
                }
                if((mem[q  ]==0x84)&&
                   (mem[q+1]==0x01)&&
                   (mem[q+2]==0x4c))
                {
                    Unp->Forced=q;
                    Unp->EndAdr=mem[q+3]|mem[q+4]<<8;
                    break;
                }
            }
            Unp->DepAdr=0x340;
            Unp->StrMem=0x801;
            PrintInfo(Unp,_I_IGP2);
            Unp->IdFlag=1;return;
        }
    }
    /* 711 introdes 3 */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x1118)==0x8D01A978) &&
           (*(unsigned int*)(mem+0x111c)==0x1A8DDC0D) &&
           (*(unsigned int*)(mem+0x1174)==0x11534C2A) &&
           (*(unsigned int*)(mem+0x2B30)==0x786001F0) )
        {
            Unp->Forced=0x2b33;
            /* it restores $3fff by saving it to $02 =) */
            mem[2]=mem[0x3fff];
            if(*(unsigned int*)(mem+0x2B4F)==0xE803409D)
            {
               Unp->DepAdr=0x340;
               Unp->StrMem=mem[0x2b65]|mem[0x2b69]<<8;
               Unp->EndAdr=Unp->info->end - (mem[0x2b5d]|mem[0x2b61]<<8) + Unp->StrMem;
               p=0x2b7f;
               if(*(unsigned int*)(mem+p-1)==0x4CA65920)
               {
                   mem[p-1]=0x2c;
                   p+=3;
               }
               Unp->RetAdr=mem[p]|mem[p+1]<<8;
            }
            PrintInfo(Unp,_I_I711ID3);
            Unp->IdFlag=1;return;
        }
    }
    /* BN 1872 intromaker(?), magic disk and so on */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x4800)==0x48F02078) &&
           (*(unsigned int*)(mem+0x4E67)==0x48004CFC) &&
           (*(unsigned int*)(mem+0x4851)==0x4E00BD78) &&
           (*(unsigned int*)(mem+0x4855)==0xE800FA9D) )
        {
            Unp->Forced=0x4851;
            Unp->DepAdr=0xfa;
            Unp->EndAdr=0x2d;
            Unp->StrMem=0x800;
            PrintInfo(Unp,_I_IBN1872);
            Unp->IdFlag=1;return;
        }
    }
    /* generic excell/ikari reloc routine */
    if( Unp->DepAdr==0 )
    {
        for (p=0xe00;p<0x3400;p++)
        {
            if((*(unsigned int*)(mem+p+0x00)==0x01A90385) &&
               (*(unsigned int*)(mem+p+0x04)==0x08A90485) &&
               (*(unsigned int*)(mem+p+0x10)==0xE6F9D0C8) )
            {
                for(q=p+0x4e;q<p+0x70;q++)
                {
                    if((*(unsigned int*)(mem+q)&0xffffff00)==0x00206C00)
                    {
                        q=1;
                        break;
                    }
                }
                if(q!=1)
                    break;

                if((mem[p-0x100+0xec]==0xa2)  &&
                   (mem[p-0x100+0xee]==0xbd)  &&
                   (*(unsigned int*)(mem+p-0x100+0xe8)==0x018534A9) &&
                   (*(unsigned int*)(mem+p-0x100+0xf1)==0xE804009D) &&
                   (*(unsigned int*)(mem+p-0x100+0xf5)==0x004CF7D0) )
                {
                    Unp->Forced=p-0x100+0xe8;
                    Unp->DepAdr=0x400;
                    break;
                }
                if((mem[p-0x100+0xc8]==0xa2)  &&
                   (mem[p-0x100+0xca]==0xbd)  &&
                   (*(unsigned int*)(mem+(mem[p-0x100+0xcb]|mem[p-0x100+0xcc]<<8)+0x0f) ==0x018534A9) &&
                   (*(unsigned int*)(mem+p-0x100+0xcd)==0xA904009D) &&
                   (*(unsigned int*)(mem+p-0x100+0xe3)==0x040F4CF8) )
                {
                    Unp->Forced=p-0x100+0xc8;
                    Unp->DepAdr=0x40f;
                    break;
                }
            }
        }

        if(Unp->DepAdr)
        {
            Unp->EndAdr=mem[p+0x22]|mem[p+0x24]<<8;
            Unp->StrMem=0x801;
            for(q=p+0x29;q<p+0x39;q++)
            {
                if((mem[q]==0xa9)||(mem[q]==0xa2)||(mem[q]==0xa0)||
                   (mem[q]==0x85)||(mem[q]==0x86))
                {
                   q++;
                   continue;
                }
                if((mem[q]==0x8d)||(mem[q]==0x8e))
                {
                   q+=2;
                   continue;
                }
                if((mem[q]==0x20)||(mem[q]==0x4c))
                {
                    Unp->RetAdr=mem[q+1]|mem[q+2]<<8;
                    if((Unp->RetAdr==0xa659)&&(mem[q]==0x20))
                    {
                        mem[q]=0x2c;
                        Unp->RetAdr=0;
                        continue;
                    }
                    break;
                }
            }
            PrintInfo(Unp,_I_IEXIKARI);
            Unp->IdFlag=1;return;
        }
    }
    /* ikari-06 by tridos/ikari */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0xC00)==0xD0CAA5A2) &&
           (*(unsigned int*)(mem+0xC10)==0xE807A00C) &&
           (*(unsigned int*)(mem+0xC20)==0x201CFB20) &&
           (*(unsigned int*)(mem+0xfe8)==0x60BD80A2) &&
           (*(unsigned int*)(mem+0xd60)==0x8534A978) )
        {
            Unp->Forced=0xfe8;
            Unp->DepAdr=0x334;
            Unp->EndAdr=0x2d;
            Unp->StrMem=0x801;
            Unp->RetAdr=mem[0xda0]|mem[0xda1]<<8;
            PrintInfo(Unp,_I_IIKARI06);
            Unp->IdFlag=1;return;
        }
    }
    /* flt-01 version at $0801 */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x998)==0x78FF5B20) &&
           (*(unsigned int*)(mem+0x9a8)==0xBE8DFAB1) &&
           (*(unsigned int*)(mem+0xd3b)==0xFD152078) &&
           (*(unsigned int*)(mem+0xd4b)==0xD0CA033B) &&
           (*(unsigned int*)(mem+0xd5b)==0xA90DFD4C) )
        {
            Unp->Forced=0xd3b;
            Unp->DepAdr=0x33c;
            Unp->EndAdr=mem[0xb5c]|mem[0xb62]<<8;
            Unp->StrMem=0x801;
            Unp->RetAdr=mem[0xb6b]|mem[0xb6c]<<8;
            PrintInfo(Unp,_I_IFLT01);
            Unp->IdFlag=1;return;
        }
    }
    /* s451-09 at $0801 */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x801)==0xE67800A0) &&
           (*(unsigned int*)(mem+0x805)==0xA62DA501) &&
           (*(unsigned int*)(mem+0x82a)==0xFD91FBB1) &&
           (*(unsigned int*)(mem+0x831)==0xC6E6D00F) &&
           (*(unsigned int*)(mem+0x841)==0x61BE0004) )
        {
            /* first moves $0839-(eof) to $1000 */
            p=mem[0x837];
            if((p==0x3e)||(p==0x48))
            {
                Unp->EndAdr=mem[0x80f]|mem[0x811]<<8;
                memmove(mem+0x1000,mem+0x839,Unp->EndAdr-0x1000);
                if(p==0x3e)
                {
                    Unp->StrMem=mem[0x113c]|mem[0x113d]<<8;
                }
                else/* if(p==0x48) */
                {
                    Unp->StrMem=mem[0x114c]|mem[0x114d]<<8;
                }
                Unp->DepAdr=Unp->StrMem;
                Unp->RetAdr=Unp->StrMem;
                Unp->Forced=Unp->StrMem;
                PrintInfo(Unp,_I_IS45109);
                Unp->IdFlag=1;return;
            }
        }
    }
    /* Super_Titlemaker/DSCompware */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x820)==0x8536A978) &&
           (*(unsigned int*)(mem+0x830)==0x8570A9F9) &&
           (*(unsigned int*)(mem+0x860)==0xB100A0B5) &&
           (*(unsigned int*)(mem+0x864)==0xE60285B2) )
        {
           Unp->EndAdr=mem[0x854]|mem[0x852]<<8;
           //Unp->EndAdC=1;
           Unp->DepAdr=0x851;
           Unp->RetAdr=mem[0x88a]|mem[0x88b]<<8;
           Unp->StrMem=Unp->RetAdr;
           Unp->Forced=0x820;
           PrintInfo(Unp,_I_DSCTITMK);
           Unp->IdFlag=1;return;
        }
    }
}
