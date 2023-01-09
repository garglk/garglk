/* XTC packer */
#include "../unp64.h"
void Scn_XTC(unpstr *Unp)
{
    unsigned char *mem;
    int q=0,p;
    if ( Unp->IdFlag )
        return;
    mem=Unp->mem;
    if( Unp->DepAdr==0 )
    {
        if ( (*(unsigned short int*)(mem+0x80d)==0xE678) &&
             (*(unsigned int*)(mem+0x811)==0x1BCE0818) &&
             (*(unsigned int*)(mem+0x819)==0xC8000099) &&
             (*(unsigned int*)(mem+0x82c)==0x4CF7D0CA) &&
             mem[0x85c]==0x99)
        {
            Unp->RetAdr=mem[0x872]|mem[0x873]<<8;
            Unp->DepAdr=0x100;
            Unp->Forced=0x80d; /* the ldy #$00 can be missing, skipped */
            Unp->fEndAf=0x121;
            Unp->EndAdC=0xffff|EA_USE_Y;
            Unp->StrMem=mem[0x85d]|mem[0x85e]<<8;
            PrintInfo(Unp,_I_XTC21);
            Unp->IdFlag=1;return;
        }
    }
    /* XTC packer 1.0 & 2.2/2.4 */
    if( Unp->DepAdr==0 )
    {
        for(p=0x801;p<0x80c;p+=0x0a)
        {
            if( (*(unsigned short int*)(mem+p+0x02)==0xE678) &&
                (*(unsigned int*)(mem+p+0x07)==(unsigned int)(0xce08|((p+0x10)<<16))) &&
                (*(unsigned int*)(mem+p+0x0e)==0xC8000099) &&
                (*(unsigned int*)(mem+p+0x23)==0x4CF7D0CA) )
            {
                /* has variable codebytes so addresses varies */
                for(q=p+0x37;q<p+0x60;q+=4)
                {
                    if(mem[q]==0xc9)
                        continue;
                    if(mem[q]==0x99)
                    {
                        Unp->DepAdr=0x100;
                        break;
                    }
                    break; /* unexpected byte, get out */
                }
                break;
            }
        }
        if( Unp->DepAdr )
        {
            Unp->RetAdr=mem[q+0x16]|mem[q+0x17]<<8;
            if(*(unsigned short int*)(mem+p     )!=0x00a0)
              Unp->Forced=p+2; /* the ldy #$00 can be missing, skipped */
            else
              Unp->Forced=p;

            Unp->fEndAf=mem[q+0x7]|mem[q+0x8]<<8;Unp->fEndAf--;
            Unp->EndAdC=0xffff|EA_USE_Y;
            Unp->StrMem=mem[q+1]|mem[q+2]<<8;
            if (*(unsigned int*)(mem+q+0x1f) == 0xDDD00285)
            {
                PrintInfo(Unp,_I_XTC10);
            }
            else if(*(unsigned int*)(mem+q+0x1f) == 0xF620DFD0)
            {
                /* rockstar's 2.2+ & shade/light's 2.4 are all the same */
                PrintInfo(Unp,_I_XTC22);
            }
            else
            {   /* actually found to be Visiomizer 6.2/Zagon */
            	Unp->DepAdr=mem[p+0x27]|mem[p+0x28]<<8;
                PrintInfo(Unp,_I_VISIOM62);
            }
            Unp->IdFlag=1;return;
        }
    }
    /* XTC 2.3 / 6codezipper */
    if( Unp->DepAdr==0 )
    {
        if((*(unsigned int*)(mem+0x803)==0xB9018478) &&
           (*(unsigned int*)(mem+0x80b)==0xF7D0C8FF) &&
           (*(unsigned int*)(mem+0x81b)==0x00FC9D08) &&
           (*(unsigned int*)(mem+0x85b)==0xD0D0FFE4) )
        {
            Unp->DepAdr=mem[0x823]|mem[0x824]<<8;
            Unp->Forced=0x803;
            Unp->RetAdr=mem[0x865]|mem[0x866]<<8;
            Unp->StrMem=mem[0x850]|mem[0x851]<<8;
            Unp->EndAdC=0xffff|EA_USE_Y;
            Unp->fEndAf=0x128;
            PrintInfo(Unp,_I_XTC23 );
            Unp->IdFlag=1;return;
        }
    }
    /* XTC 2.3 / G*P, probably by Rockstar */
    if( Unp->DepAdr==0 )
    {
        if(((*(unsigned int*)(mem+0x803)==0xB901e678)||
            (*(unsigned int*)(mem+0x803)==0xB9018478))&&
           (*(unsigned int*)(mem+0x80b)==0xF7D0C8FF) &&
           (*(unsigned int*)(mem+0x81b)==0x00F59D08) &&
           (*(unsigned int*)(mem+0x85b)==0xD0D0F8E4) )
        {
            Unp->DepAdr=mem[0x823]|mem[0x824]<<8;
            Unp->Forced=0x803;
            Unp->RetAdr=mem[0x865]|mem[0x866]<<8;
            Unp->StrMem=mem[0x850]|mem[0x851]<<8;
            Unp->EndAdC=0xffff|EA_USE_Y;
            Unp->fEndAf=0x121;
            PrintInfo(Unp,_I_XTC23GP );
            Unp->IdFlag=1;return;
        }
    }
    /* XTC packer 2.x? found in G*P/NEI/Armageddon warez
    just some different byte on copy loop, else is equal to 2.3
    */
    if( Unp->DepAdr==0 )
    {
        for(p=0x801;p<0x80c;p+=0x0a)
        {
            if(((*(unsigned int*)(mem+p+0x00)&0xffff0000)==0xE6780000) &&
               ((*(unsigned int*)(mem+p+0x05)&0xffff00ff)==0xB90800CE) &&
                (*(unsigned int*)(mem+p+0x0b)==0xC8000099) &&
                (*(unsigned int*)(mem+p+0x1e)==0x4CF7D0CA) )
            {
                /* has variable codebytes so addresses varies */
                for(q=p+0x36;q<p+0x60;q+=4)
                {
                    if(mem[q]==0xc9)
                        continue;
                    if(mem[q]==0x99)
                    {
                        Unp->DepAdr=0x100;
                        break;
                    }
                    break; /* unexpected byte, get out */
                }
                break;
            }
        }
        if( Unp->DepAdr )
        {
            Unp->RetAdr=mem[q+0x16]|mem[q+0x17]<<8;
            Unp->Forced=p+2;
            Unp->fEndAf=mem[q+0x7]|mem[q+0x8]<<8;Unp->fEndAf--;
            Unp->EndAdC=0xffff|EA_USE_Y;
            Unp->StrMem=mem[q+1]|mem[q+2]<<8;
            PrintInfo(Unp,_I_XTC2XGP);
            Unp->IdFlag=1;return;
        }
    }
}
