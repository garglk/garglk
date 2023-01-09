/*
MrZ/Triad packer, does cover also some triad intros.
found also as "SYS2069 PSHYCO!" in some Censor warez.
*/
#include "../unp64.h"
void Scn_MrZ(unpstr *Unp)
{
    unsigned char *mem;
    int q,p=0;
    if ( Unp->IdFlag )
        return;
    mem=Unp->mem;
    if( Unp->DepAdr==0 )
    {
        for(q=0x810;q<0x880;q++)
        {
            if((*(unsigned int*)(mem+q    )==0x8D00A978)&&
               (*(unsigned int*)(mem+q+4  )==0x0BA9D020)&&
               (*(unsigned int*)(mem+q+8  )==0xE6D0118D)&&
               (*(unsigned short int*)(mem+q+0xc)==(unsigned short int )0x4c01) )
            {
                p=mem[q+0xe]|mem[q+0xf]<<8;
                Unp->Forced=q;
                break;
            }
            if((*(unsigned int*)(mem+q    )==0x208D00A9)&&
               (*(unsigned int*)(mem+q+4  )==0x8D0BA9D0)&&
               (*(unsigned int*)(mem+q+8  )==0xE678D011)&&
               (*(unsigned short int*)(mem+q+0xc)==(unsigned short int )0x4c01) )
            {
                p=mem[q+0xe]|mem[q+0xf]<<8;
                Unp->Forced=q;
                break;
            }
            if((*(unsigned int*)(mem+q    )==0xD9409901)&&
               (*(unsigned int*)(mem+q+4  )==0xD0FCC6C8)&&
               (*(unsigned int*)(mem+q+8  )==0xDFD0E8ED)&&
               (*(unsigned int*)(mem+q+0xc)==0x4C01E678) )
            {
                p=mem[q+0x10]|mem[q+0x11]<<8;
                break;
            }
            /* mrz crunch, identical apart preamble? %) */
            if((*(unsigned int*)(mem+q+0x00)==0x6FD0116F)&&
               (*(unsigned int*)(mem+q+0x0a)==0xFACA0820)&&
               (*(unsigned int*)(mem+q+0x1b)==0x3737261F)&&
               (*(unsigned int*)(mem+q+0x42)==0x4C01E678) )
            {
                p=mem[q+0x46]|mem[q+0x47]<<8;
                break;
            }
        }
        if(p)
        {
            if((*(unsigned int*)(mem+p     )==0xA29AFAA2)&&
               (*(unsigned int*)(mem+p+0x08)==0x10CA3995)&&
               (*(unsigned int*)(mem+p+0x2b)==0xB901004C)
              )
            {
                Unp->DepAdr=0x100;
                Unp->EndAdr=mem[p+0x36]|mem[p+0x37]<<8;
                if(Unp->EndAdr==0)
                    Unp->EndAdr=0x10000;
                Unp->fStrAf=0x41; /*str_add=EA_USE_Y; Y is stored at $6d*/
                for(q=p+0x8c;q<Unp->info->end;q++)
                {
                    if((mem[q]==0xa9)||(mem[q]==0xc6))
                    {
                        q++;
                        continue;
                    }
                    if(mem[q]==0x8d)
                    {
                        q+=2;
                        continue;
                    }
                    if((mem[q]==0x20)&&(
                      (*(unsigned short int*)(mem+q+1)==0xe3bf)||
                      (*(unsigned short int*)(mem+q+1)==0xfda3)||
                      (*(unsigned short int*)(mem+q+1)==0xa660)||
                      (*(unsigned short int*)(mem+q+1)==0xa68e)||
                      (*(unsigned short int*)(mem+q+1)==0xa659)))
                    {
                        mem[q]=0x2c;
                        q+=2;
                        continue;
                    }
                    if(mem[q]==0x4c)
                    {
                        Unp->RetAdr=mem[q+1]|mem[q+2]<<8;
                        //if(Unp->DebugP)
                            //printf("Startaddres patch # $%04x\n",q);
                        p=2;
                        mem[p++]=0xA5; /* fix for start address */
                        mem[p++]=0x41; /*     *=$02             */
                        mem[p++]=0x18; /*     lda $41           */
                        mem[p++]=0x65; /*     clc               */
                        mem[p++]=0x6D; /*     adc $6d           */
                        mem[p++]=0x85; /*     sta $41           */
                        mem[p++]=0x41; /*     bcc noh           */
                        mem[p++]=0x90; /*     inc $42           */
                        mem[p++]=0x02; /* noh rts               */
                        mem[p++]=0xE6; /*                       */
                        mem[p++]=0x42; /*                       */
                        mem[p++]=0x4c; /*     jmp realstart     */
                        mem[p++]=mem[q+1];
                        mem[p++]=mem[q+2];

                        mem[q+0]=0x4c;
                        mem[q+1]=0x02;
                        mem[q+2]=0x00;

                        break;
                    }
                }
                //if(Unp->DebugP)
                    //printf("Retaddr $%04x\n",Unp->RetAdr);
                PrintInfo(Unp,_I_MRZPACK);
                Unp->IdFlag=1;return;
            }
        }
    }
}
