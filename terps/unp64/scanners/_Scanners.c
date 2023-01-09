#include "../unp64.h"

Scnptr ScanFunc[] = {
    //        Scn_NotPackers
    //    ,Scn_Autostarters
    //    ,Scn_XIP
    //    ,Scn_OUG
    //    ,Scn_IsePic
    //    ,Scn_Optimus
    Scn_ECA
    //    ,Scn_xCodeZippers
    ,
    Scn_Expert
    //    ,Scn_AR
    //    ,Scn_BitImploder
    //    ,Scn_Jox
    //    ,Scn_ExplCrunch
    //    ,Scn_ExplFaces
    //    ,Scn_Facepacker
    //    ,Scn_ABCruncher
    //    ,Scn_BYG
    //    ,Scn_FilecompactorTMC
    //    ,Scn_ASCprot
    //    ,Scn_CFB
    //    ,Scn_Zipper
    //    ,Scn_NSU
    //    ,Scn_IDT
    ,
    Scn_Cruel,
    Scn_PuCrunch
    //    ,Scn_MDG
    ,Scn_AbuzeCrunch
    //    ,Scn_SledgeHammer
    //    ,Scn_TimCrunch
    //    ,Scn_TimeCruncher
    //    ,Scn_BetaDynamic
    //    ,Scn_DarkSqueezer
    ,
    Scn_ByteBoiler,
    Scn_MrCross
    //    ,Scn_1001card
    //    ,Scn_WDRsoftp
    //    ,Scn_DSCcoder
    //    ,Scn_BonanzaBros
    //    ,Scn_Agony
    //    ,Scn_ExplXRated
    //    ,Scn_ByteCompactor
    ,
    Scn_MasterCompressor
    //    ,Scn_ScreenCrunch
    //    ,Scn_MCCrackenComp
    ,Scn_FinalSuperComp
    //    ,Scn_4cPack
    //    ,Scn_FXbytepress
    //    ,Scn_FXbitstream
    //    ,Scn_Trianon
    //    ,Scn_KompressMaster711
    //    ,Scn_TurboPacker
    ,
    Scn_TCScrunch,
    Scn_TBCMultiComp
    //    ,Scn_ByteBoozer
    //    ,Scn_ALZ64
          ,Scn_XTC
    //    ,Scn_UniPacker
    //    ,Scn_TCD
    //    ,Scn_Matcham
    //    ,Scn_Supercrunch
    //    ,Scn_Superpack
    //    ,Scn_BeefTrucker
    //    ,Scn_TSB
    //    ,Scn_ISC
    //    ,Scn_TMM
    //    ,Scn_FilecompactorMTB
    //    ,Scn_EqByteComp
    //    ,Scn_Galleon
    //    ,Scn_BeastLink
    //    ,Scn_BronxPacker
    //    ,Scn_Oneway
    //    ,Scn_CeleriPack
    //    ,Scn_CadgersPacker
    //    ,Scn_Crush
    //    ,Scn_Lightmizer
    //    ,Scn_Frog
    //    ,Scn_FrontPacker
    //    ,Scn_MartinPiper
    //    ,Scn_Apack
    //    ,Scn_ONS
    //    ,Scn_FC3pack
    //    ,Scn_Amnesia
    //    ,Scn_Polonus
    //    ,Scn_KressCrunch
    //    ,Scn_SirMiniPack
    //    ,Scn_MRD
    ,
    Scn_CCS
    //    ,Scn_PZW
    //    ,Scn_C4MRP
    //    ,Scn_ILSCoder
    //    ,Scn_MSI
    //    ,Scn_U_111pack
    //    ,Scn_Hawk
    //    ,Scn_TSMcoder
    //    ,Scn_Ikari
    //    ,Scn_WGIcoder
    //    ,Scn_HTLcoder
    //    ,Scn_XDScoder
    //    ,Scn_FDTcoder
    //    ,Scn_SKLcoder
    //    ,Scn_SpeediComp
    //    ,Scn_U_DSC_MA
    //    ,Scn_P100
    //    ,Scn_MaschSprache
    //    ,Scn_MegaCruncher
    //    ,Scn_VIP
    //    ,Scn_Gandalf
    //    ,Scn_AEKcoder
    //    ,Scn_LSTcoder
    //    ,Scn_Eastlinker
    //    ,Scn_Jazzcat
    //    ,Scn_PITcoder
    //    ,Scn_BitPacker
    //    ,Scn_Rows
    //    ,Scn_U_400All
    //    ,Scn_U_439pack
    //    ,Scn_64er
    //    ,Scn_TMCcoder
    //    ,Scn_ALScoder
    //    ,Scn_UltraComp
    //    ,Scn_FCG
    ,
    Scn_Megabyte
    //    ,Scn_Zigag
    //    ,Scn_Brains
    //    ,Scn_Graffity
    ,
    Scn_Section8
        ,Scn_MrZ
    //    ,Scn_DD
    //    ,Scn_PackOpt
    //    ,Scn_Warlock
    //    ,Scn_U_100pack
    //    ,Scn_U_101pack
    //    ,Scn_SyncroPack
    //    ,Scn_Relax
    //    ,Scn_Jazoo
    //    ,Scn_WCC
    //    ,Scn_LTS
    //    ,Scn_Low
    //    ,Scn_C_Noack
    //    ,Scn_Koncina
    //    ,Scn_U_Triad
    //    ,Scn_RapEq
    //    ,Scn_ByteKiller
    //    ,Scn_Loadstar
    //    ,Scn_Trashcan
          ,Scn_Caution
    //    ,Scn_U_25_pack
    //    ,Scn_U_8e_pack
    //    ,Scn_U_P3_pack
    //    ,Scn_FalcoPack
//    ,Scn_FP
//    ,Scn_FinalCompactor
    //    ,Scn_NEC
    //    ,Scn_EnigmaMFFL
    //    ,Scn_Shurigen
    //    ,Scn_STL
    //    ,Scn_GrafBinaer
    //    ,Scn_AbyssCoder
    //    ,Scn_SPC
    //    ,Scn_FSW
    //    ,Scn_BAM
    //    ,Scn_Cobra
    //    ,Scn_ByteBuster
    //    ,Scn_Mekker
    //    ,Scn_ByteStrainer
    //    ,Scn_Jedi
    //    ,Scn_Pride
    //    ,Scn_StarCrunch
    //    ,Scn_SpyPack
    //    ,Scn_GPacker
    //    ,Scn_Cadaver
    //    ,Scn_Helmet
    //    ,Scn_Excalibur
    //    ,Scn_CNCD
    //    ,Scn_Anticom
    //    ,Scn_Antiram
    //    ,Scn_BN1872
    //    ,Scn_4wd
    //    ,Scn_Huffer
    //    ,Scn_NM156
    //    ,Scn_Ratt
    //    ,Scn_Byterapers
    //    ,Scn_CIACrypt
    //    ,Scn_PAN
    //    ,Scn_TKC
    //    ,Scn_UProt
    //    ,Scn_YetiCoder
    //    ,Scn_Panoramic
    //    ,Scn_THS
    //    ,Scn_WHO
    //    ,Scn_KGBcoder
    //    ,Scn_Pyra
    ,
    Scn_ActionPacker
    //    ,Scn_CCrypt
    //    ,Scn_TDT
    //    ,Scn_Cult
    //    ,Scn_Gimzo
    //    ,Scn_BlackAdder
    //    ,Scn_ICS8
    //    ,Scn_Paradroid
    //    ,Scn_Plasma
    //    ,Scn_DrZoom
    //    ,Scn_ExactCoder
    //    ,Scn_CNETFixer
    //    ,Scn_Triangle
    //    ,Scn_SFLinker
    //    ,Scn_Bongo
    //    ,Scn_Jemasoft
    //    ,Scn_TATCoder
    //    ,Scn_RFOCoder
    //    ,Scn_Doynamite
    //    ,Scn_Nibbit
    //    ,Scn_TLRLinker
    //    ,Scn_TLRSubsizer
    //    ,Scn_AdmiralP4kbar
    //    ,Scn_ZeroCoder
    //    ,Scn_NuCrunch
    //    ,Scn_TinyCrunch
    //    ,Scn_FileLinkerSDA
    //    ,Scn_Inceria
    ,
    Scn_Exomizer
        ,Scn_Intros
    //    ,Scn_U_Generic801
};

void Scanners(unpstr *Unp) {
  int x, y;
  y = sizeof(ScanFunc) / sizeof(*ScanFunc);
  for (x = 0; x < y; x++) {
    (ScanFunc[x])(Unp);
    if (Unp->IdFlag)
      break;
  }
}
/* all this to make a central strings pool with unique const strings
   else every module would have own local strings, duplicated many times.
*/
void PrintInfo(unpstr *Unp, int id) {
  switch (id) {
        case _I_1001_4:
        fprintf(stderr, DEPMASK2,"1001 CardCruncher","v4.x",Unp->DepAdr);
        break;
        case _I_1001_NEWPACK:
        fprintf(stderr, DEPMASK,"1001 CardCruncher New Packer",Unp->DepAdr);
        break;
        case _I_1001_OLDPACK:
        fprintf(stderr, DEPMASK,"1001 CardCruncher Old Packer",Unp->DepAdr);
        break;
        case _I_1001_CRAM:
        fprintf(stderr, DEPMASK2,"1001 CardCruncher","CRAM",Unp->DepAdr);
        break;
        case _I_1001_ACM:
        fprintf(stderr, DEPMASK2,"1001 CardCruncher","ACM",Unp->DepAdr);
        break;
        case _I_1001_HTL:
        fprintf(stderr, DEPMASK2,"1001 CardCruncher","HTL",Unp->DepAdr);
        break;
    //    case _I_DATELUC:
    //    fprintf(stderr, DEPMASK2,"Datel
    //    UltraCompander","Cruncher",Unp->DepAdr); break; case _I_DATELUCP:
    //    fprintf(stderr, DEPMASK2,"Datel UltraCompander","Packer",Unp->DepAdr);
    //    break;
    //    case _I_DATELUCC:
    //    fprintf(stderr, DEPMASK2,"Datel
    //    UltraCompander","Compactor",Unp->DepAdr); break; case _I_4CPACK:
    //    fprintf(stderr, DEPMASK2,"4C","Packer",Unp->DepAdr);
    //    break;
    //    case _I_64ER_SP22:
    //    sprintf(appstr,"%s %s","/ special","v2.2");
    //    fprintf(stderr, DEPMASK2,"64er",appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ER_SP23:
    //    sprintf(appstr,"%s %s","/ special","v2.3");
    //    fprintf(stderr, DEPMASK2,"64er",appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ER_SP2U:
    //    sprintf(appstr,"%s %s","/ special","v2.?");
    //    fprintf(stderr, DEPMASK2,"64er",appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ER_SP12:
    //    sprintf(appstr,"%s %s","/ special","v1.2");
    //    fprintf(stderr, DEPMASK2,"64er",appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ER_SP13:
    //    sprintf(appstr,"%s %s","/ special","v1.3");
    //    fprintf(stderr, DEPMASK2,"64er",appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ER_SP14:
    //    sprintf(appstr,"%s %s","/ special","v1.4");
    //    fprintf(stderr, DEPMASK2,"64er",appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ER_SP15:
    //    sprintf(appstr,"%s %s","/ special","v1.5");
    //    fprintf(stderr, DEPMASK2,"64er",appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ER_SP1U:
    //    sprintf(appstr,"%s %s","/ special","v1.?");
    //    fprintf(stderr, DEPMASK2,"64er",appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ERBITP:
    //    sprintf(appstr,"%s%s","Bit","Packer");
    //    fprintf(stderr, DEPMASK2,"64er",appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ERBITP1:
    //    sprintf(appstr,"%s%s","Bit","Packer");
    //    strcat(appstr," ");
    //    strcat(appstr,"/ special");
    //    fprintf(stderr, DEPMASK2,"64er",appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ERS41A:
    //    strcpy(appstr,"64er");
    //    strcat(appstr," ");
    //    strcat(appstr,"/ Equal sequences");
    //    strcat(appstr," ");
    //    strcat(appstr,"v4.1");strcat(appstr,"A");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ERS41B:
    //    strcpy(appstr,"64er");
    //    strcat(appstr," ");
    //    strcat(appstr,"/ Equal sequences");
    //    strcat(appstr," ");
    //    strcat(appstr,"v4.1");strcat(appstr,"B");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ERS41C:
    //    strcpy(appstr,"64er");
    //    strcat(appstr," ");
    //    strcat(appstr,"/ Equal sequences");
    //    strcat(appstr," ");
    //    strcat(appstr,"v4.1");strcat(appstr,"C");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ERS40A:
    //    strcpy(appstr,"64er");
    //    strcat(appstr," ");
    //    strcat(appstr,"/ Equal sequences");
    //    strcat(appstr," ");
    //    strcat(appstr,"v4.0");strcat(appstr,"A");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    break;
    //    case _I_64ERS40B:
    //    strcpy(appstr,"64er");
    //    strcat(appstr," ");
    //    strcat(appstr,"/ Equal sequences");
    //    strcat(appstr," ");
    //    strcat(appstr,"v4.0");strcat(appstr,"B");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ERS40C:
    //    strcpy(appstr,"64er");
    //    strcat(appstr," ");
    //    strcat(appstr,"/ Equal sequences");
    //    strcat(appstr," ");
    //    strcat(appstr,"v4.0");strcat(appstr,"C");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ERS40D:
    //    strcpy(appstr,"64er");
    //    strcat(appstr," ");
    //    strcat(appstr,"/ Equal sequences");
    //    strcat(appstr," ");
    //    strcat(appstr,"v4.0");strcat(appstr,"D");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ERS40DB:
    //    strcpy(appstr,"64er");
    //    strcat(appstr," ");
    //    strcat(appstr,"/ Equal sequences");
    //    strcat(appstr," ");
    //    strcat(appstr,"v4.0");strcat(appstr,"DB");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ERS40F:
    //    strcpy(appstr,"64er");
    //    strcat(appstr," ");
    //    strcat(appstr,"/ Equal sequences");
    //    strcat(appstr," ");
    //    strcat(appstr,"v4.0");strcat(appstr,"F");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ER_15:
    //    fprintf(stderr, DEPMASK2,"64er","v1.5",Unp->DepAdr);
    //    break;
    //    case _I_64ER_21:
    //    fprintf(stderr, DEPMASK2,"64er","v2.1",Unp->DepAdr);
    //    break;
    //    case _I_HAPPYS32:
    //    strcpy(appstr,"Happy-");strcat(appstr,"Packer");
    //    strcat(appstr," ");
    //    strcat(appstr,"/ Equal sequences");
    //    strcat(appstr," ");
    //    strcat(appstr,"v3.2");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_HAPPYS22:
    //    strcpy(appstr,"Happy-");strcat(appstr,"Packer");
    //    strcat(appstr," ");
    //    strcat(appstr,"/ Equal sequences");
    //    strcat(appstr," ");
    //    strcat(appstr,"v2.2");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_64ERAUTO:
    //    fprintf(stderr, DEPMASK2,"64er","Autostarter",Unp->DepAdr);
    //    break;
    //    case _I_AB_CRUNCH:
    //    sprintf(appstr,"%s%s","AB","Cruncher");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    case _I_ABUZECRUNCH:
    fprintf(stderr, DEPMASK,"AbuzeCrunch",Unp->DepAdr);
    break;
    case _I_ABUZECR37:
        fprintf(stderr, DEPMASK2,"AbuzeCrunch","v3.7",Unp->DepAdr);
        break;
    case _I_ABUZECR50:
        fprintf(stderr, DEPMASK2,"AbuzeCrunch","v5.0",Unp->DepAdr);
        break;
    //    case _I_AEKCOD20 :
    //    sprintf(appstr,"%s %s","AEK","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_AEKCOD11 :
    //    sprintf(appstr,"%s %s","AEK","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.1",Unp->DepAdr);
    //    break;
    //    case _I_SYNCRO13:
    //    sprintf(appstr,"%s %s","Syncro","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.3",Unp->DepAdr);
    //    break;
    //    case _I_SYNCRO14:
    //    sprintf(appstr,"%s %s","Syncro","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.4",Unp->DepAdr);
    //    break;
    //    case _I_SYNCRO155:
    //    sprintf(appstr,"%s %s","Syncro","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.55",Unp->DepAdr);
    //    break;
    //    case _I_SYNCRO12 :
    //    sprintf(appstr,"%s %s","Syncro","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.2",Unp->DepAdr);
    //    break;
    //    case _I_TLRPACK:
    //    fprintf(stderr, DEPMASK2,"T.L.R.","Packer",Unp->DepAdr);
    //    break;
    //    case _I_TLRPROT1:
    //    sprintf(appstr,"%s %s","T.L.R.","Protector");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_TLRPROT2:
    //    sprintf(appstr,"%s %s","T.L.R.","Protector");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_TLRLINK:
    //    fprintf(stderr, DEPMASK2,"T.L.R.","Linker",Unp->DepAdr);
    //    break;
    //    case _I_TLRLINKFM:
    //    sprintf(appstr,"%s %s","T.L.R.","Linker");
    //    fprintf(stderr, DEPMASK2,appstr,"FullMem",Unp->DepAdr);
    //    break;
    //    case _I_TLRSSIZER:
    //    fprintf(stderr, DEPMASK2,"T.L.R.","Subsizer",Unp->DepAdr);
    //    break;
    //    case _I_TLRSSIZD5:
    //    sprintf(appstr,"%s %s","T.L.R.","Subsizer");
    //    strcat(appstr," 0.5");
    //    fprintf(stderr, DEPMASK2,appstr,"/ dirty",Unp->DepAdr);
    //    break;
    //    case _I_TLRSSIZD6:
    //    sprintf(appstr,"%s %s","T.L.R.","Subsizer");
    //    strcat(appstr," 0.6");
    //    fprintf(stderr, DEPMASK2,appstr,"/ dirty",Unp->DepAdr);
    //    break;
    //    case _I_AGONYPACK:
    //    sprintf(appstr,"%s %s","Agony","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_DARKP20:
    //    sprintf(appstr,"%s %s","Dark","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_DARKP21:
    //    sprintf(appstr,"%s %s","Dark","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.1",Unp->DepAdr);
    //    break;
    //    case _I_DARKP31:
    //    sprintf(appstr,"%s %s","Dark","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.1",Unp->DepAdr);
    //    break;
    //    case _I_JAP2:
    //    sprintf(appstr,"%s %s","Just A","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_ALSCODER:
    //    fprintf(stderr, DEPMASK2,"ALS","Coder",Unp->DepAdr);
    //    break;
    //    case _I_ALZ64:
    //    fprintf(stderr, DEPMASK,"ALZ64/Kabuto",Unp->DepAdr);
    //    break;
    //    case _I_AMNESIA1:
    //    sprintf(appstr,"%s %s","Amnesia","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_AMNESIA2:
    //    sprintf(appstr,"%s %s","Amnesia","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_APACK:
    //    fprintf(stderr, DEPMASK,"Apack/MadRom",Unp->DepAdr);
    //    break;
    //    case _I_GPACK:
    //    fprintf(stderr, DEPMASK,"G-Packer/Oxyron",Unp->DepAdr);
    //    break;
    //    case _I_AR4  :
    //    fprintf(stderr, DEPMASK2,"Action Replay","v4.x",Unp->DepAdr);
    //    break;
    //    case _I_AR3  :
    //    fprintf(stderr, DEPMASK2,"Action Replay","v3.x",Unp->DepAdr);
    //    break;
    //    case _I_ARU  :
    //    fprintf(stderr, DEPMASK2,"Action Replay","Unknown",Unp->DepAdr);
    //    break;
    //    case _I_AR42F:
    //    sprintf(appstr,"%s %s","Action Replay","v4.x");
    //    fprintf(stderr, DEPMASK2,appstr,"Split Freeze",Unp->DepAdr);
    //    break;
    //    case _I_SSNAP:
    //    sprintf(appstr,"%s %s","Super","Snapshot");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_FRZMACH:
    //    fprintf(stderr, DEPMASK,"Freeze Machine",Unp->DepAdr);
    //    break;
    //    case _I_FRZMACH2F:
    //    fprintf(stderr, DEPMASK2,"Freeze Machine","Split Freeze",Unp->DepAdr);
    //    break;
    //    case _I_ASCPROT:
    //    fprintf(stderr, DEPMASK2,"ASC/SCF","Protector",Unp->DepAdr);
    //    break;
    //    case _I_BEASTLINK:
    //    fprintf(stderr, DEPMASK,"BeastLink",Unp->DepAdr);
    //    break;
    //    case _I_BEEFTR56:
    //    fprintf(stderr, DEPMASK2,"BeefTrucker","$56",Unp->DepAdr);
    //    break;
    //    case _I_BEEFTR54:
    //    fprintf(stderr, DEPMASK2,"Zipplink","$54",Unp->DepAdr);
    //    break;
    //    case _I_BEEFTR2:
    //    strcpy(appstr,"BeefTrucker");strcat(appstr,"/");strcat(appstr,"Zipplink");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_BEEFTLB:
    //    strcpy(appstr,"BeefTrucker");strcat(appstr,"/");strcat(appstr,"Zipplink");
    //    fprintf(stderr, DEPMASK2,appstr,"LoadBack",Unp->DepAdr);
    //    break;
    //    case _I_ZIPLINK21:
    //    fprintf(stderr, DEPMASK2,"Zip Link","v2.1",Unp->DepAdr);
    //    break;
    //    case _I_ZIPLINK25:
    //    fprintf(stderr, DEPMASK2,"Zip Link","v2.5",Unp->DepAdr);
    //    break;
    //    case _I_BETADYN3:
    //    sprintf(appstr,"%s %s","Beta Dynamic","Compressor");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.x",Unp->DepAdr);
    //    break;
    //    case _I_BETADYN3FX:
    //    sprintf(appstr,"%s %s","Beta Dynamic","Compressor");
    //    strcat(appstr," ");
    //    strcat(appstr,"v3.x");strcat(appstr,"/");strcat(appstr,"FX");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_BETADYN3CCS:
    //    sprintf(appstr,"%s %s","Beta Dynamic","Compressor");
    //    strcat(appstr," ");
    //    strcat(appstr,"v3.x");strcat(appstr,"/");strcat(appstr,"CCS");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_BETADYN2:
    //    sprintf(appstr,"%s %s","Beta Dynamic","Compressor");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_BITIMP:
    //    sprintf(appstr,"%s%s","Bit","Imploder");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_BITPACK2:
    //    sprintf(appstr,"%s%s","Bit","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_BONANZA:
    //    fprintf(stderr, DEPMASK2,"BonanzaBros","Coder",Unp->DepAdr);
    //    break;
    //    case _I_BRAINS:
    //    fprintf(stderr, DEPMASK2,"Brains","Packer",Unp->DepAdr);
    //    break;
    //    case _I_BRONX:
    //    fprintf(stderr, DEPMASK2,"Bronx","Packer",Unp->DepAdr);
    //    break;
    //    case _I_BYG1:
    //    sprintf(appstr,"%s %s","BYG","Compactor");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_BYG2:
    //    sprintf(appstr,"%s %s","BYG","Compactor");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_BYGBB:
    //    fprintf(stderr, DEPMASK2,"Byte","Baby",Unp->DepAdr);
    //    break;
  case _I_BYTEBOILER:
    fprintf(stderr, DEPMASK, "ByteBoiler", Unp->DepAdr);
    break;
  case _I_BYTEBOICPX:
    fprintf(stderr, DEPMASK2, "ByteBoiler", "CPX", Unp->DepAdr);
    break;
  case _I_BYTEBOISCS:
    fprintf(stderr, DEPMASK2, "ByteBoiler", "SCS", Unp->DepAdr);
    break;
    //    case _I_BYTEBOOZER:
    //    sprintf(appstr,"%s%s","Byte","Boozer");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_BYTEBOOZ20:
    //    sprintf(appstr,"%s%s","Byte","Boozer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_BYTEBOOZ11C:
    //    sprintf(appstr,"%s%s","Byte","Boozer");
    //    strcat(appstr," ");
    //    strcat(appstr,"v1.1");strcat(appstr,"C");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_BYTEBOOZ11E:
    //    sprintf(appstr,"%s%s","Byte","Boozer");
    //    strcat(appstr," ");
    //    strcat(appstr,"v1.1");strcat(appstr,"E");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_LAMEBOOZER:
    //    sprintf(appstr,"%s%s","Lamer","Boozer");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_BYTECOMP:
    //    sprintf(appstr,"%s%s","Byte","Compactor");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_BYTEKILL:
    //    sprintf(appstr,"%s%s","Byte","Killer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_C4MRP:
    //    fprintf(stderr, DEPMASK,"C4MRP/RSi",Unp->DepAdr);
    //    break;
    //    case _I_CADGERS:
    //    fprintf(stderr, DEPMASK2,"Cadgers","Packer",Unp->DepAdr);
    //    break;
        case _I_CAUTION10:
        fprintf(stderr, DEPMASK2,"QuickPacker","v1.0",Unp->DepAdr);
        break;
        case _I_CAUTION25:
        fprintf(stderr, DEPMASK2,"QuickPacker","v2.5",Unp->DepAdr);
        break;
        case _I_CAUTION25SS:
        fprintf(stderr, DEPMASK2,"QuickPacker/Sysless","v2.5",Unp->DepAdr);
        break;
        case _I_CAUTION20:
        fprintf(stderr, DEPMASK2,"QuickPacker","v2.0",Unp->DepAdr);
        break;
        case _I_CAUTION20SS:
        fprintf(stderr, DEPMASK2,"QuickPacker/Sysless","v2.0",Unp->DepAdr);
        break;
        case _I_CAUTIONHP:
        fprintf(stderr, DEPMASK2,"HardPacker","v1.0",Unp->DepAdr);
        break;
    //    case _I_CCSMAXS:
    //    fprintf(stderr, DEPMASK2,"CCS","MaxShorter",Unp->DepAdr);
    //    break;
        case _I_CCSCRUNCH1:
        fprintf(stderr, DEPMASK2,"CCS Cruncher","v1.x",Unp->DepAdr);
        break;
        case _I_CCSCRUNCH2:
        fprintf(stderr, DEPMASK2,"CCS Cruncher","v2.x",Unp->DepAdr);
        break;
        case _I_CCSPACK:
        fprintf(stderr, DEPMASK2,"CCS","Packer",Unp->DepAdr);
        break;
    //    case _I_CCSSCRE:
    //    fprintf(stderr, DEPMASK2,"CCS ScreenShorter","v1.0",Unp->DepAdr);
    //    break;
    //    case _I_CCSSPEC:
    //    fprintf(stderr, DEPMASK2,"CCS","Special",Unp->DepAdr);
    //    break;
    //    case _I_CCSEXEC:
    //    fprintf(stderr, DEPMASK2,"CCS","Executer",Unp->DepAdr);
    //    break;
    //    case _I_CCSUNKN:
    //    fprintf(stderr, DEPMASK2,"CCS","Unknown",Unp->DepAdr);
    //    break;
    //    case _I_CCSHACK:
    //    fprintf(stderr, DEPMASK2,"CCS","MaxShorter Hack",Unp->DepAdr);
    //    break;
    //    case _I_CELERIP:
    //    fprintf(stderr, DEPMASK,"CeleriPack",Unp->DepAdr);
    //    break;
    //    case _I_CFBCOD1:
    //    sprintf(appstr,"%s %s","CFB","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_CFBCOD2:
    //    sprintf(appstr,"%s %s","CFB","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
  case _I_CRUEL22:
    fprintf(stderr, DEPMASK2, "CruelCrunch", "v2.2", Unp->DepAdr);
    break;
  case _I_CRUEL2MHZ:
    fprintf(stderr, DEPMASK, "CruelCrunch v2.2+ / 2MHZ", Unp->DepAdr);
    break;
  case _I_CRUELFAST2:
    fprintf(stderr, DEPMASK, "CruelCrunch v2.5 / fast", Unp->DepAdr);
    break;
  case _I_CRUELFAST4:
    fprintf(stderr, DEPMASK, "CruelCrunch v4.0 / fast", Unp->DepAdr);
    break;
  case _I_CRUEL_X:
    fprintf(stderr, DEPMASK2, "CruelCrunch", "vx.x", Unp->DepAdr);
    break;
  case _I_CRUELMS15:
    fprintf(stderr, DEPMASK2, "MSCrunch", "v1.5", Unp->DepAdr);
    break;
  case _I_CRUELMS10:
    fprintf(stderr, DEPMASK2, "MSCrunch", "v1.0", Unp->DepAdr);
    break;
  case _I_CRUELMS1X:
    fprintf(stderr, DEPMASK2, "MSCrunch", "v1.x", Unp->DepAdr);
    break;
  case _I_CRUELMS_U:
    fprintf(stderr, DEPMASK2, "MSCrunch", "Unknown", Unp->DepAdr);
    break;
    //    case _I_CRUMSABS:
    //    sprintf(appstr,"%s / %s","v1.5","ABS");
    //    fprintf(stderr, DEPMASK2,"MSCrunch",appstr,Unp->DepAdr);
    //    break;
  case _I_CRUEL_HDR:
    fprintf(stderr, DEPMASK, "CruelCrunch header", Unp->DepAdr);
    break;
  case _I_CRUEL_H22:
    fprintf(stderr, DEPMASK2, "CruelCrunch header", "v2.2", Unp->DepAdr);
    break;
  case _I_CRUEL_H20:
    fprintf(stderr, DEPMASK2, "CruelCrunch header", "v2.0", Unp->DepAdr);
    break;
  case _I_CRUEL_HTC:
    fprintf(stderr, DEPMASK2, "CruelCrunch header", "TCOM", Unp->DepAdr);
    break;
  case _I_CRUEL20:
    fprintf(stderr, DEPMASK2, "CruelCrunch", "v2.0", Unp->DepAdr);
    break;
  case _I_CRUEL21:
    fprintf(stderr, DEPMASK2, "CruelCrunch", "v2.1", Unp->DepAdr);
    break;
  case _I_CRUEL_ILS:
    fprintf(stderr, DEPMASK, "CruelCrunch v2.x / ILS", Unp->DepAdr);
    break;
  case _I_CRUEL_RND:
    fprintf(stderr, DEPMASK, "CruelCrunch v2.x / RND", Unp->DepAdr);
    break;
  case _I_CRUEL10:
    fprintf(stderr, DEPMASK2, "CruelCrunch", "v1.0", Unp->DepAdr);
    break;
  case _I_CRUEL12:
    fprintf(stderr, DEPMASK2, "CruelCrunch", "v1.2", Unp->DepAdr);
    break;
  case _I_CRUEL14:
    fprintf(stderr, DEPMASK, "CruelCrunch v1.4 / Light", Unp->DepAdr);
    break;
  case _I_CRUEL_TKC:
    fprintf(stderr, DEPMASK, "CruelCrunch v2.3 / TKC", Unp->DepAdr);
    break;
    //    case _I_TABOOCRUSH:
    //    fprintf(stderr, DEPMASK2,"Crush","/ Taboo",Unp->DepAdr);
    //    break;
    //    case _I_TABOOCRNCH:
    //    sprintf(appstr,"%s %s","Unknown","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"/ Taboo",Unp->DepAdr);
    //    break;
    //    case _I_TABOOPACK:
    //    sprintf(appstr,"%s %s","Unknown","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"/ Taboo",Unp->DepAdr);
    //    break;
    //    case _I_TABOOPACK64:
    //    sprintf(appstr,"%s %s","Packer","64");
    //    fprintf(stderr, DEPMASK2,appstr,"/ Taboo",Unp->DepAdr);
    //    break;
    //    case _I_CNOACK:
    //    sprintf(appstr,"%s %s","Chris Noack","Mega");
    //    strcat(appstr," ");strcat(appstr,"Byte");
    //    fprintf(stderr, DEPMASK2,appstr,"Packer",Unp->DepAdr);
    //    break;
    //    case _I_CNOACKLXT:
    //    sprintf(appstr,"%s %s","Chris Noack","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"/ Laxity",Unp->DepAdr);
    //    break;
    //    case _I_DARKSQ08  :
    //    fprintf(stderr, DEPMASK2,"DarkSqueezer","v0.8",Unp->DepAdr);
    //    break;
    //    case _I_DARKSQ21  :
    //    fprintf(stderr, DEPMASK2,"DarkSqueezer","v2.1",Unp->DepAdr);
    //    break;
    //    case _I_DARKSQ2X  :
    //    fprintf(stderr, DEPMASK2,"DarkSqueezer","v2.x",Unp->DepAdr);
    //    break;
    //    case _I_DARKSQ4   :
    //    fprintf(stderr, DEPMASK2,"DarkSqueezer","v4.x",Unp->DepAdr);
    //    break;
    //    case _I_DARKSQF4  :
    //    sprintf(appstr,"%s / %s","DarkSqueezer","F4CG");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
        case _I_DARKSQXTC :
        fprintf(stderr, DEPMASK,"DarkSqueezer / XTC",Unp->DepAdr);
        break;
    //    case _I_DARKSQWOW:
    //    sprintf(appstr,"%s / %s","DarkSqueezer","Darkfiler");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_DARKSQGP  :
    //    sprintf(appstr,"%s / %s","DarkSqueezer","G*P");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_DARKSQDOM  :
    //    sprintf(appstr,"%s / %s","DarkSqueezer","DOM");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_DARKSQPAR  :
    //    sprintf(appstr,"%s / %s","DarkSqueezer","Paradroid");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_DARKSQ4SHK :
    //    sprintf(appstr,"%s / %s","DarkSqueezer","Sharks");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_DARKSQ22:
    //    fprintf(stderr, DEPMASK2,"DarkSqueezer","v2.2",Unp->DepAdr);
    //    break;
    //    case _I_DARKSQBB1 :
    //    sprintf(appstr,"%s / %s","DarkSqueezer","ByteBonker");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_DARKSQBB2 :
    //    sprintf(appstr,"%s / %s","DarkSqueezer","ByteBonker");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_DDINTROC :
    //    fprintf(stderr, DEPMASK2,"DD Intro","Coder",Unp->DepAdr);
    //    break;
    //    case _I_DSCCOD :
    //    fprintf(stderr, DEPMASK2,"DSCompware","Coder",Unp->DepAdr);
    //    break;
    //    case _I_EASTLINK :
    //    fprintf(stderr, DEPMASK2,"East","Linker",Unp->DepAdr);
    //    break;
  case _I_ECA:
    fprintf(stderr, DEPMASK2, "ECA", "Compacker", Unp->DepAdr);
    break;
  case _I_ECAOLD:
    fprintf(stderr, DEPMASK2, "ECA", "/ old?", Unp->DepAdr);
    break;
    //    case _I_ENIGMAMFFL:
    //    fprintf(stderr, DEPMASK,"Enigma MFFL",Unp->DepAdr);
    //    break;
    //    case _I_EQBYTEC12:
    //    sprintf(appstr,"%s %s","Equal Byte","Compressor");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.2",Unp->DepAdr);
    //    break;
    //    case _I_EQBYTEC14:
    //    sprintf(appstr,"%s %s","Equal Byte","Compressor");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.4",Unp->DepAdr);
    //    break;
    //    case _I_EQBYTEC17:
    //    sprintf(appstr,"%s %s","Equal Byte","Compressor");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.7",Unp->DepAdr);
    //    break;
    //    case _I_EQBYTEC19:
    //    sprintf(appstr,"%s %s","Equal Byte","Compressor");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.9",Unp->DepAdr);
    //    break;
    //    case _I_EQBYTEC19TAL:
    //    sprintf(appstr,"%s %s","Equal Byte","Compressor");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.9/TAL",Unp->DepAdr);
    //    break;
  case _I_EXOM:
    fprintf(stderr, DEPMASK, "Exomizer", Unp->DepAdr);
    break;
  case _I_EXOM3:
    fprintf(stderr, DEPMASK2, "Exomizer", "v3.0", Unp->DepAdr);
    break;
  case _I_EXOM302:
    fprintf(stderr, DEPMASK2, "Exomizer", "v3.02+", Unp->DepAdr);
    break;
  case _I_EXPERT1X:
    fprintf(stderr, DEPMASK2, "Trilogic Expert", "v1.x", Unp->DepAdr);
    break;
  case _I_EXPERT27:
    fprintf(stderr, DEPMASK2, "Trilogic Expert", "v2.7", Unp->DepAdr);
    break;
  case _I_EXPERT29:
    fprintf(stderr, DEPMASK2, "Trilogic Expert", "v2.9", Unp->DepAdr);
    break;
  case _I_EXPERT29EUC:
    fprintf(stderr, DEPMASK2, "Trilogic Expert v2.9a/EUC", appstr, Unp->DepAdr);
    break;
  case _I_EXPERT2X:
    fprintf(stderr, DEPMASK2, "Trilogic Expert", "v2.x", Unp->DepAdr);
    break;
  case _I_EXPERT20:
    fprintf(stderr, DEPMASK2, "Trilogic Expert", "v2.0", Unp->DepAdr);
    break;
  case _I_EXPERT21:
    fprintf(stderr, DEPMASK, "Trilogic Expert v2.10MMC", Unp->DepAdr);
    break;
  case _I_EXPERT4X:
    fprintf(stderr, DEPMASK2, "Trilogic Expert", "v4.x", Unp->DepAdr);
    break;
  case _I_EXPERT3X:
    fprintf(stderr, DEPMASK2, "Trilogic Expert", "v3.x", Unp->DepAdr);
    break;
  case _I_EXPERTASS:
    fprintf(stderr, DEPMASK, "Trilogic Expert / ASS", Unp->DepAdr);
    break;
  case _I_EXPERT211:
    fprintf(stderr, DEPMASK2, "Trilogic Expert", "v2.11", Unp->DepAdr);
    break;
    //    case _I_EXPLCRUNCH1X:
    //    sprintf(appstr,"%s %s","Exploding","CruelCrunch"); appstr[15]=0;
    //    fprintf(stderr, DEPMASK2,appstr,"v2.6",Unp->DepAdr);
    //    break;
    //    case _I_EXPLCRUNCH2X:
    //    sprintf(appstr,"%s %s","Exploding","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_EXPLCRUNCH21:
    //    sprintf(appstr,"%s %s","Exploding","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.1",Unp->DepAdr);
    //    break;
    //    case _I_EXPLCRUNCH22:
    //    sprintf(appstr,"%s %s","Exploding","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.2",Unp->DepAdr);
    //    break;
    //    case _I_EXPLCOD:
    //    fprintf(stderr, DEPMASK2,"Exploding","Coder",Unp->DepAdr);
    //    break;
    //    case _I_EXPLFAC1    :
    //    sprintf(appstr,"%s %s","Exploding","Faces");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_EXPLFAC2    :
    //    sprintf(appstr,"%s %s","Exploding","Faces");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_XRPOWC71    :
    //    sprintf(appstr,"%s %s","PowerCrunch","v7.1");
    //    fprintf(stderr, DEPMASK2,"X-Rated",appstr,Unp->DepAdr);
    //    break;
    //    case _I_XREXPLCR    :
    //    sprintf(appstr,"%s %s","Exploding","Cruncher");
    //    fprintf(stderr, DEPMASK2,"X-Rated",appstr,Unp->DepAdr);
    //    break;
    //    case _I_XRPOWC74    :
    //    sprintf(appstr,"%s %s","PowerCrunch","v7.4");
    //    fprintf(stderr, DEPMASK2,"X-Rated",appstr,Unp->DepAdr);
    //    break;
    //    case _I_FACEPACK10:
    //    strcpy(appstr,"Face");strcat(appstr,"Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_FACEPACK11:
    //    strcpy(appstr,"Face");strcat(appstr,"Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.1",Unp->DepAdr);
    //    break;
    //    case _I_FC3PACK:
    //    sprintf(appstr,"%s %s","FinalCart","III");
    //    fprintf(stderr, DEPMASK2,appstr,"Packer",Unp->DepAdr);
    //    break;
    //    case _I_FC3LD:
    //    sprintf(appstr,"%s %s","FinalCart","III");
    //    fprintf(stderr, DEPMASK2,appstr,"Split Freeze",Unp->DepAdr);
    //    break;
    //    case _I_FC2LD:
    //    sprintf(appstr,"%s %s","FinalCart","II");
    //    fprintf(stderr, DEPMASK2,appstr,"Split Freeze",Unp->DepAdr);
    //    break;
    //    case _I_FCU1LD:
    //    sprintf(appstr,"%s %s","FinalCart","v1.?");
    //    fprintf(stderr, DEPMASK2,appstr,"Split Freeze",Unp->DepAdr);
    //    break;
    //    case _I_FCULD:
    //    sprintf(appstr,"%s %s","FinalCart","Unknown");
    //    fprintf(stderr, DEPMASK2,appstr,"Split Freeze",Unp->DepAdr);
    //    break;
    //    case _I_FCGPACK10:
    //    sprintf(appstr,"%s %s","FCG","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_FCGPACK30:
    //    sprintf(appstr,"%s %s","FCG","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.0",Unp->DepAdr);
    //    break;
    //    case _I_FCGPACK12:
    //    sprintf(appstr,"%s %s","FCG","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.2",Unp->DepAdr);
    //    break;
    //    case _I_FCGPACK13:
    //    sprintf(appstr,"%s %s","FCG","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.3",Unp->DepAdr);
    //    break;
    //    case _I_FCGLINK:
    //    fprintf(stderr, DEPMASK2,"FCG","Linker",Unp->DepAdr);
    //    break;
    //    case _I_TFGPACK:
    //    fprintf(stderr, DEPMASK2,"TFG","Packer",Unp->DepAdr);
    //    break;
    //    case _I_TFGCODE:
    //    fprintf(stderr, DEPMASK2,"TFG","Coder",Unp->DepAdr);
    //    break;
    //    case _I_TMCULINK:
    //    sprintf(appstr,"%s %s","Ultimate","Linker");
    //    fprintf(stderr, DEPMASK2,"TMC",appstr,Unp->DepAdr);
    //    break;
    //    case _I_FCGCODER:
    //    fprintf(stderr, DEPMASK2,"FCG","Coder",Unp->DepAdr);
    //    break;
    //    case _I_HLEISEP:
    //    fprintf(stderr, DEPMASK2,"H.Leise","Protector",Unp->DepAdr);
    //    break;
    //    case _I_FCGPROT:
    //    fprintf(stderr, DEPMASK2,"FCG","Protector",Unp->DepAdr);
    //    break;
    //    case _I_EXPROT:
    //    strcpy(appstr,"Ex");strcat(appstr,"Protector");appstr[9]=0;
    //    fprintf(stderr, DEPMASK2,"FCG",appstr,Unp->DepAdr);
    //    break;
    //    case _I_FLSPROT33:
    //    sprintf(appstr,"%s %s","Flash","Protector");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.3",Unp->DepAdr);
    //    break;
    //    case _I_FCCPROT:
    //    fprintf(stderr, DEPMASK2,"FCC","Protector",Unp->DepAdr);
    //    break;
    //    case _I_FDTCOD:
    //    fprintf(stderr, DEPMASK2,"FDT","Coder",Unp->DepAdr);
    //    break;
    //    case _I_FCOMPMTB:
    //    strcpy(appstr,"File");strcat(appstr,"Compactor");
    //    fprintf(stderr, DEPMASK2,appstr,"I/MTB",Unp->DepAdr);
    //    break;
    //    case _I_FCOMPTMC:
    //    sprintf(appstr,"%s %s","/ Flexible"+2,"Code");
    //    sprintf(appstr,"%s %s","/ Flexible","Code");
    //    fprintf(stderr, DEPMASK2,appstr,"Compactor",Unp->DepAdr);
    //    break;
    //    case _I_FCOMPMTC:
    //    fprintf(stderr, DEPMASK2,"MeanTeam","Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_FCOMPK2:
    //    sprintf(appstr,"%s %s","/ Flexible"+2,"Code");
    //    sprintf(appstr,"%s %s","/ Flexible","Code");
    //    strcat(appstr," "); strcat(appstr,"Compactor");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_FCOMPSYS3:
    //    sprintf(appstr,"%s %s","/ Flexible"+2,"Code");
    //    sprintf(appstr,"%s %s","/ Flexible","Code");
    //    strcat(appstr," "); strcat(appstr,"Compactor");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.x",Unp->DepAdr);
    //    break;
    case _I_FINCOMP:
    fprintf(stderr, DEPMASK2,"Final","Compactor",Unp->DepAdr);
    break;
        case _I_SUPCOMFLEX:
        fprintf(stderr, DEPMASK,"Super Compressor / Flexible",Unp->DepAdr);
        break;
        case _I_SUPCOMEQSE:
        fprintf(stderr, DEPMASK,"Super Compressor / Equal sequences",Unp->DepAdr);
        break;
        case _I_SUPCOMEQB9:
        fprintf(stderr, DEPMASK2,"Super Compressor / Equal sequences","Hack",Unp->DepAdr);
        break;
        case _I_SUPCOMEQCCS:
        fprintf(stderr, DEPMASK2,"Super Compressor / Equal sequences","CCS",Unp->DepAdr);
        break;
        case _I_SUPCOMEQC9:
        fprintf(stderr, DEPMASK2,"Super Compressor / Equal chars","Hack",Unp->DepAdr);
        break;
        case _I_SUPCOMEQCH:
        fprintf(stderr, DEPMASK,"Super Compressor / Equal chars",Unp->DepAdr);
        break;
    //    case _I_SUPCOMFH11:
    //    sprintf(appstr,"%s %s %s %c %s %c","Super","Compressor","Hack",
    //    '1',"layer",'1'); fprintf(stderr, DEPMASK,appstr,Unp->DepAdr); break;
    //    case _I_SUPCOMFH12:
    //    sprintf(appstr,"%s %s %s %c %s %c","Super","Compressor","Hack",
    //    '1',"layer",'2'); fprintf(stderr, DEPMASK,appstr,Unp->DepAdr); break;
    //    case _I_SUPCOMFH13:
    //    sprintf(appstr,"%s %s %s %c %s %c","Super","Compressor","Hack",
    //    '1',"layer",'3'); fprintf(stderr, DEPMASK,appstr,Unp->DepAdr); break;
    //    case _I_SUPCOMFH21:
    //    sprintf(appstr,"%s %s %s %c %s %c","Super","Compressor","Hack",
    //    '2',"layer",'1'); fprintf(stderr, DEPMASK,appstr,Unp->DepAdr); break;
    //    case _I_SUPCOMHACK:
    //    sprintf(appstr,"%s %s","Super","Compressor");
    //    strcat(appstr," "); strcat(appstr,"/ Flexible");
    //    fprintf(stderr, DEPMASK2,appstr,"Hack",Unp->DepAdr);
    //    break;
    //    case _I_FPCOD:
    //    fprintf(stderr, DEPMASK2,"FP","Coder",Unp->DepAdr);
    //    break;
    //    case _I_FROGPACK:
    //    fprintf(stderr, DEPMASK2,"Frog","Packer",Unp->DepAdr);
    //    break;
    //    case _I_FRONTPACK:
    //    fprintf(stderr, DEPMASK2,"Front","Packer",Unp->DepAdr);
    //    break;
    //    case _I_FXBYTEP:
    //    sprintf(appstr,"%s %s","FX","Bytepress");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_FXBP_BBSP:
    //    sprintf(appstr,"%s %s","/ BB","(Soundpacker)");
    //    fprintf(stderr, DEPMASK2,"FX Bytepress",appstr,Unp->DepAdr);
    //    break;
    //    case _I_FXBP_BB:
    //    fprintf(stderr, DEPMASK2,"FX Bytepress","/ BB",Unp->DepAdr);
    //    break;
    //    case _I_FXBP_JW:
    //    fprintf(stderr, DEPMASK2,"FX Bytepress","/ Jewels",Unp->DepAdr);
    //    break;
    //    case _I_FXBP_SN:
    //    fprintf(stderr, DEPMASK2,"FX Bytepress","/ Seen",Unp->DepAdr);
    //    break;
    //    case _I_FXBITST:
    //    fprintf(stderr, DEPMASK2,"FX","Bitstreamer",Unp->DepAdr);
    //    break;
    //    case _I_BITST11:
    //    fprintf(stderr, DEPMASK2,"Bitstreamer","v1.1",Unp->DepAdr);
    //    break;
    //    case _I_GALLEONEQ:
    //    fprintf(stderr, DEPMASK2,"Galleon","/ Equal chars",Unp->DepAdr);
    //    break;
    //    case _I_GALLCP35:
    //    sprintf(appstr,"%s %s","Galleon","Compactor");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.5",Unp->DepAdr);
    //    break;
    //    case _I_GALLCP36:
    //    sprintf(appstr,"%s %s","Galleon","Compactor");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.6",Unp->DepAdr);
    //    break;
    //    case _I_GALLCP37:
    //    sprintf(appstr,"%s %s","Galleon","Compactor");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.7",Unp->DepAdr);
    //    break;
    //    case _I_GALLCP38:
    //    sprintf(appstr,"%s %s","Galleon","Compactor");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.8",Unp->DepAdr);
    //    break;
    //    case _I_GALLCP39:
    //    sprintf(appstr,"%s %s","Galleon","Compactor");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.9",Unp->DepAdr);
    //    break;
    //    case _I_GALLCP3X:
    //    sprintf(appstr,"%s %s","Galleon","Compactor");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.x",Unp->DepAdr);
    //    break;
    //    case _I_GALLFW4C:
    //    fprintf(stderr, DEPMASK2,"Galleon","FW4C",Unp->DepAdr);
    //    break;
    //    case _I_GALLFW4C2:
    //    sprintf(appstr,"%s %s","Galleon","FW4C");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_GALLFW4C3:
    //    sprintf(appstr,"%s %s","Galleon","FW4C");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.0",Unp->DepAdr);
    //    break;
    //    case _I_GALLFW4C31:
    //    sprintf(appstr,"%s %s","Galleon","FW4C");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.1",Unp->DepAdr);
    //    break;
    //    case _I_GALLBRPR:
    //    strcpy(appstr,"Byterapers");appstr[9]=0;
    //    fprintf(stderr, DEPMASK2,"Galleon",appstr,Unp->DepAdr);
    //    break;
    //    case _I_GALLU02:
    //    sprintf(appstr,"%s %s","Galleon","$02");
    //    fprintf(stderr, DEPMASK2,appstr,"Packer",Unp->DepAdr);
    //    break;
    //    case _I_GALLU100:
    //    sprintf(appstr,"%s %s","Galleon","$100");
    //    fprintf(stderr, DEPMASK2,appstr,"Packer",Unp->DepAdr);
    //    break;
    //    case _I_GALLWHOM1:
    //    sprintf(appstr,"%s %s","Galleon","Whom");
    //    strcat(appstr," ");strcat(appstr,"Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_GALLWHOM11:
    //    sprintf(appstr,"%s %s","Galleon","Whom");
    //    strcat(appstr," ");strcat(appstr,"Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.1",Unp->DepAdr);
    //    break;
    //    case _I_GALLWHOM2:
    //    sprintf(appstr,"%s %s","Galleon","Whom");
    //    strcat(appstr," ");strcat(appstr,"Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_GALLWHOM4:
    //    sprintf(appstr,"%s %s","Galleon","Whom");
    //    strcat(appstr," ");strcat(appstr,"Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v4.x",Unp->DepAdr);
    //    break;
    //    case _I_GALLWHOM4S:
    //    sprintf(appstr,"%s %s","Galleon","Whom");
    //    strcat(appstr," ");strcat(appstr,"Packer");
    //    strcat(appstr," ");strcat(appstr,"v4.x");strcat(appstr,"/sysless");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_GALLVEST:
    //    fprintf(stderr, DEPMASK,"Alvesta Link",Unp->DepAdr);
    //    break;
    //    case _I_GANDALF:
    //    fprintf(stderr, DEPMASK2,"Gandalf","Packer",Unp->DepAdr);
    //    break;
    //    case _I_GRAFFITY:
    //    fprintf(stderr, DEPMASK2,"Graffity","Packer",Unp->DepAdr);
    //    break;
    //    case _I_HAWK:
    //    fprintf(stderr, DEPMASK2,"Hawk","Packer",Unp->DepAdr);
    //    break;
    //    case _I_HTLCOD:
    //    fprintf(stderr, DEPMASK2,"HTL","Coder",Unp->DepAdr);
    //    break;
    //    case _I_IDT10:
    //    sprintf(appstr,"%s %s","Idiots","FX");
    //    strcat(appstr," ");strcat(appstr,"Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_IDTFX21:
    //    sprintf(appstr,"%s %s","Idiots","FX");
    //    strcat(appstr," ");strcat(appstr,"Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.1",Unp->DepAdr);
    //    break;
    //    case _I_IDTFX20:
    //    sprintf(appstr,"%s %s","Idiots","FX");
    //    strcat(appstr," ");strcat(appstr,"Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_GNTFX20:
    //    sprintf(appstr,"%s %s","Gentlemen","FX");
    //    strcat(appstr," ");strcat(appstr,"Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_GNTSTATP:
    //    sprintf(appstr,"%s %s","Gentlemen","Stat");
    //    fprintf(stderr, DEPMASK2,appstr,"Packer",Unp->DepAdr);
    //    break;
    //    case _I_EXCELCOD:
    //    fprintf(stderr, DEPMASK2,"Excell/Ikari","Coder",Unp->DepAdr);
    //    break;
    //    case _I_IKARICR:
    //    fprintf(stderr, DEPMASK2,"Ikari","Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_ILSCOD:
    //    strcpy(appstr,"ILS");strcat(appstr,"/");strcat(appstr,"RND");
    //    fprintf(stderr, DEPMASK2,appstr,"Coder",Unp->DepAdr);
    //    break;
    //    case _I_IRELAX1:
    //    sprintf(appstr,"%s %s","Intro:","Relax");
    //    strcat(appstr,"-01");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_IRELAX2:
    //    sprintf(appstr,"%s %s","Intro:","Relax");
    //    strcat(appstr,"-02");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_IF4CG23:
    //    sprintf(appstr,"%s %s","Intro:","F4CG");
    //    strcat(appstr,"-23");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_ITRIAD1:
    //    sprintf(appstr,"%s %s","Intro:","Triad");
    //    strcat(appstr,"-01");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_ITRIAD2:
    //    sprintf(appstr,"%s %s","Intro:","Triad");
    //    strcat(appstr,"-02");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_ITRIAD5:
    //    sprintf(appstr,"%s %s","Intro:","Triad");
    //    strcat(appstr,"-05");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_IGP2:
    //    sprintf(appstr,"%s %s","Intro:","G*P");
    //    strcat(appstr,"-02");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_I711ID3:
    //    sprintf(appstr,"%s %s","711","IntroDesigner3");
    //    fprintf(stderr, DEPMASK2,"Intro:",appstr,Unp->DepAdr);
    //    break;
    //    case _I_IBN1872:
    //    fprintf(stderr, DEPMASK2,"Intro:","BN 1872",Unp->DepAdr);
    //    break;
    //    case _I_BN1872PK:
    //    fprintf(stderr, DEPMASK2,"BN 1872","Packer",Unp->DepAdr);
    //    break;
    //    case _I_IEXIKARI:
    //    sprintf(appstr,"%s %s","Intro:","Excell/Ikari");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_IIKARI06:
    //    sprintf(appstr,"%s %s","Intro:","Ikari");
    //    strcat(appstr,"-06");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_IFLT01:
    //    sprintf(appstr,"%s %s","Intro:","FLT");
    //    strcat(appstr,"-01");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_IS45109:
    //    sprintf(appstr,"%s %s","Intro:","S451");
    //    strcat(appstr,"-09");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_ISCN:
    //    sprintf(appstr,"%s %s", "Normal","Cruncher");
    //    fprintf(stderr, DEPMASK2,"ISC /",appstr,Unp->DepAdr);
    //    break;
    //    case _I_ISCP:
    //    fprintf(stderr, DEPMASK2,"ISC /","Packer",Unp->DepAdr);
    //    break;
    //    case _I_ISCBS:
    //    sprintf(appstr,"%s %s", "Bitstream","Cruncher");
    //    fprintf(stderr, DEPMASK2,"ISC /",appstr,Unp->DepAdr);
    //    break;
    //    case _I_ISEPIC:
    //    fprintf(stderr, DEPMASK,"IsePic",Unp->DepAdr);
    //    break;
    //    case _I_ISEPDD:
    //    sprintf(appstr,"%sker/DD","IsePic");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_ISEPCT1:
    //    sprintf(appstr,"Why %s/CT","IsePic");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_ISEPCT2:
    //    sprintf(appstr,"Why %s/CT","IsePic");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_ISEP7U1:
    //    sprintf(appstr,"Anti-%s / %s %s","IsePic", "7up","v1.x");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_ISEP7U2:
    //    sprintf(appstr,"Anti-%s / %s %s","IsePic", "7up","v2.x");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_ISEPGS1:
    //    sprintf(appstr,"Anti-%s / %s %s","IsePic", "GSS","v1.1");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_ISEPNC:
    //    strcpy(appstr,"Defroster");strcat(appstr,"/");strcat(appstr,"NC");
    //    fprintf(stderr, DEPMASK2,"IsePic",appstr,Unp->DepAdr);
    //    break;
    //    case _I_ISEPAMG:
    //    strcpy(appstr,"Defroster");strcat(appstr,"/");strcat(appstr,"Amigo");
    //    fprintf(stderr, DEPMASK2,"IsePic",appstr,Unp->DepAdr);
    //    break;
    //    case _I_ISEPGEN:
    //    fprintf(stderr, DEPMASK2,"Generic","IsePic",Unp->DepAdr);
    //    break;
    //    case _I_JAZOOCOD:
    //    fprintf(stderr, DEPMASK2,"Jazoo","Coder",Unp->DepAdr);
    //    break;
    //    case _I_JCTCR:
    //    fprintf(stderr, DEPMASK2,"JCT","Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_JCTPACK:
    //    fprintf(stderr, DEPMASK2,"JCT","Packer",Unp->DepAdr);
    //    break;
    //    case _I_JOXCR:
    //    fprintf(stderr, DEPMASK2,"J0xstrap","Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_KOMPSP:
    /* eq sequence 4.1A/4.3a */
    //    fprintf(stderr, DEPMASK2,"Kompressmaster","/ special",Unp->DepAdr);
    //    break;
    //    case _I_KOMP711:
    //    /* eq sequence 4.1B */
    //    sprintf(appstr,"%s / %s","Kompressmaster","711");
    //    fprintf(stderr, DEPMASK2,appstr,"Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_KOMPBB:
    //    sprintf(appstr,"%s / %s","Kompressmaster","BB");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_KOMP71PS:
    //    /* packer after 4.1a/4.3a */
    //    sprintf(appstr,"%s %s","Kompressmaster","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"/ special",Unp->DepAdr);
    //    break;
    //    case _I_KOMP71P1:
    //    /* packer 4.3a */
    //    sprintf(appstr,"%s %s","Kompressmaster","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_KOMP71P2:
    //    /* packer 4.1 */
    //    sprintf(appstr,"%s %s","Kompressmaster","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_KONCINA:
    //    fprintf(stderr, DEPMASK2,"Koncina","Packer",Unp->DepAdr);
    //    break;
    //    case _I_LIGHTM:
    //    fprintf(stderr, DEPMASK,"Lightmizer",Unp->DepAdr);
    //    break;
    //    case _I_VISIOM63:
    //    fprintf(stderr, DEPMASK2,"Visiomizer","v6.3",Unp->DepAdr);
    //    break;
    //    case _I_VISIOM62:
    //    fprintf(stderr, DEPMASK2,"Visiomizer","v6.2",Unp->DepAdr);
    //    break;
    //    case _I_LSMODL:
    //    sprintf(appstr,"%s %s","Loadstar","Mod");
    //    fprintf(stderr, DEPMASK2,appstr,"Linker",Unp->DepAdr);
    //    break;
    //    case _I_LSPACK:
    //    fprintf(stderr, DEPMASK2,"Loadstar","Packer",Unp->DepAdr);
    //    break;
    //    case _I_LSLNK2:
    //    sprintf(appstr,"%s %s","Loadstar","Linker");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_LOWCR:
    //    fprintf(stderr, DEPMASK2,"Low","Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_LSTCOD1:
    //    sprintf(appstr,"%s %s","LST","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_LSTCOD2:
    //    sprintf(appstr,"%s %s","LST","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_LTSPACK:
    //    fprintf(stderr, DEPMASK2,"LTS","Packer",Unp->DepAdr);
    //    break;
    //    case _I_LZMPI1:
    //    fprintf(stderr, DEPMASK2,"LZMPi/MartinPiper","v1.x",Unp->DepAdr);
    //    break;
    //    case _I_LZMPI2:
    //    fprintf(stderr, DEPMASK2,"LZMPi/MartinPiper","v2.x",Unp->DepAdr);
    //    break;
    //    case _I_MASCHK:
    //    fprintf(stderr, DEPMASK,"MaschinenSprache-Kompressor",Unp->DepAdr);
    //    break;
  case _I_MASTCOMP:
    fprintf(stderr, DEPMASK, "Master Compressor", Unp->DepAdr);
    break;
  case _I_MASTCRLX:
    fprintf(stderr, DEPMASK2,  "Master Compressor /", "Relax", Unp->DepAdr);
    break;
  case _I_MASTCAGL:
    fprintf(stderr, DEPMASK2, "Master Compressor /", "Agile", Unp->DepAdr);
    break;
  case _I_MASTCHF:
    fprintf(stderr, DEPMASK2, "Master Compressor v3.5 /", "Channel4", Unp->DepAdr);
    break;
    //    case _I_MATCHARP:
    //    sprintf(appstr,"%s%s","Char","Packer");
    //    fprintf(stderr, DEPMASK2,"Matcham",appstr,Unp->DepAdr);
    //    break;
    //    case _I_MATCBASP:
    //    sprintf(appstr,"%s%s","Basic","Packer");
    //    fprintf(stderr, DEPMASK2,"Matcham",appstr,Unp->DepAdr);
    //    break;
    //    case _I_MATCDNEQ:
    //    sprintf(appstr,"%s %s","Matcham","Denser");
    //    fprintf(stderr, DEPMASK2,appstr,"/ Equal chars",Unp->DepAdr);
    //    break;
    //    case _I_MATCDN25:
    //    sprintf(appstr,"%s %s","Matcham","Denser");
    //    fprintf(stderr, DEPMASK2,appstr,"/ 251",Unp->DepAdr);
    //    break;
    //    case _I_MATCDNFL:
    //    sprintf(appstr,"%s %s","Matcham","Denser");
    //    fprintf(stderr, DEPMASK2,appstr,"/ Flexible",Unp->DepAdr);
    //    break;
    //    case _I_MATCLNK2:
    //    sprintf(appstr,"%s %s","Matcham","Linker");
    //    fprintf(stderr, DEPMASK2,appstr,"/ 251",Unp->DepAdr);
    //    break;
    //    case _I_MATCFLEX:
    //    fprintf(stderr, DEPMASK2,"Matcham","Flexer",Unp->DepAdr);
    //    break;
    //    case _I_MCCCOMP:
    //    fprintf(stderr, DEPMASK2,"MC-Cracken","Compressor",Unp->DepAdr);
    //    break;
    //    case _I_MCTSS3  :
    //    fprintf(stderr, DEPMASK2,"TSS v3","(MC-CC hack)",Unp->DepAdr);
    //    break;
    //    case _I_MCTSSIP :
    //    fprintf(stderr, DEPMASK2,"TSS v3","IsePic",Unp->DepAdr);
    //    break;
    //    case _I_MCUNK   :
    //    fprintf(stderr, DEPMASK2,"Unknown","(MC-CC hack)",Unp->DepAdr);
    //    break;
    //    case _I_MCCRUSH3:
    //    fprintf(stderr, DEPMASK2,"Crusher v3.0","(MC-CC hack)",Unp->DepAdr);
    //    break;
    //    case _I_MCCOBRA :
    //    fprintf(stderr, DEPMASK2,"Cobra","(MC-CC hack)",Unp->DepAdr);
    //    break;
    //    case _I_MCCSCC  :
    //    fprintf(stderr, DEPMASK2,"SCC","(MC-CC hack)",Unp->DepAdr);
    //    break;
    //    case _I_MCCRYPT :
    //    fprintf(stderr, DEPMASK2,"MC-Cracken","Coder",Unp->DepAdr);
    //    break;
    //    case _I_MDGPACK:
    //    fprintf(stderr, DEPMASK2,"MDG","Packer",Unp->DepAdr);
    //    break;
    //    case _I_MBCR1:
    //    sprintf(appstr,"%s%s","Mega","Byte");
    //    strcat(appstr," ");
    //    strcat(appstr,"Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_MBCR2:
    //    sprintf(appstr,"%s%s","Mega","Byte");
    //    strcat(appstr," ");
    //    strcat(appstr,"Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_MEGCRBD:
    //    sprintf(appstr,"%s%s","Mega","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"/ BlackDuke",Unp->DepAdr);
    //    break;
    //    case _I_MEGCR23:
    //    sprintf(appstr,"%s%s","Mega","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.3",Unp->DepAdr);
    //    break;
    //    case _I_MRCROSS1:
    //    fprintf(stderr, DEPMASK2,"Mr.Cross","v1.x",Unp->DepAdr);
    //    break;
        case _I_MRCROSS2:
        fprintf(stderr, DEPMASK2,"Mr.Cross","v2.x",Unp->DepAdr);
        break;
    //    case _I_MRDCR1:
    //    sprintf(appstr,"%s %s","Marauder","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_MRDCRCOD1:
    //    sprintf(appstr,"%s %s","Marauder","Cruncher");
    //    strcat(appstr,"&");strcat(appstr,"Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_MRDLNK2:
    //    sprintf(appstr,"%s %s","Marauder","Linker");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_GSSCO12:
    //    sprintf(appstr,"%s %s","GSS","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.2",Unp->DepAdr);
    //    break;
        case _I_MRZPACK:
        fprintf(stderr, DEPMASK2,"Mr.Z","Packer",Unp->DepAdr);
        break;
    //    case _I_MSICR2:
    //    sprintf(appstr,"%s %s","MSI","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_MSICR3:
    //    sprintf(appstr,"%s %s","MSI","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.x",Unp->DepAdr);
    //    break;
    //    case _I_MSICOD:
    //    fprintf(stderr, DEPMASK2,"MSI","Coder",Unp->DepAdr);
    //    break;
    //    case _I_NECPACK:
    //    fprintf(stderr, DEPMASK2,"NEC","Packer",Unp->DepAdr);
    //    break;
    //    case _I_AUSTROV1:
    //    fprintf(stderr, DEPMASK2,"Austro-Comp","v1.0",Unp->DepAdr);
    //    break;
    //    case _I_AUSTROV2:
    //    fprintf(stderr, DEPMASK2,"Austro-Comp","v2.0",Unp->DepAdr);
    //    break;
    //    case _I_AUSTROE1:
    //    fprintf(stderr, DEPMASK2,"Austro-Comp","E1-J/Simons",Unp->DepAdr);
    //    break;
    //    case _I_AUSTRO88:
    //    fprintf(stderr, DEPMASK2,"AustroSpeed","88",Unp->DepAdr);
    //    break;
    //    case _I_BLITZCOM:
    //    fprintf(stderr, DEPMASK2,"Blitz","Compiler",Unp->DepAdr);
    //    break;
    //    case _I_AUSTRO_U:
    //    fprintf(stderr, DEPMASK2,"Austro/Blitz","Unknown",Unp->DepAdr);
    //    break;
    //    case _I_AUSTROS1:
    //    fprintf(stderr, DEPMASK2,"AustroSpeed","v1.0",Unp->DepAdr);
    //    break;
    //    case _I_AUSTROBL:
    //    fprintf(stderr, DEPMASK2,"Austro/Blitz","Compiler",Unp->DepAdr);
    //    break;
    //    case _I_PETSPEED:
    //    fprintf(stderr, DEPMASK2,"PetSpeed","Compiler",Unp->DepAdr);
    //    break;
    //    case _I_BASIC64 :
    //    fprintf(stderr, DEPMASK2,"Basic64","Compiler",Unp->DepAdr);
    //    break;
    //    case _I_BASICBS :
    //    fprintf(stderr, DEPMASK2,"BasicBoss","Compiler",Unp->DepAdr);
    //    break;
    //    case _I_SPEEDCM :
    //    fprintf(stderr, DEPMASK2,"Speed","Compiler",Unp->DepAdr);
    //    break;
    //    case _I_SPEEDY64:
    //    fprintf(stderr, DEPMASK2,"Speedy 64","Compiler",Unp->DepAdr);
    //    break;
    //    case _I_CC65    :
    //    fprintf(stderr, DEPMASK2,"CC65","Compiler",Unp->DepAdr);
    //    break;
    //    case _I_CC6522  :
    //    sprintf(appstr,"%s %s","CC65","v2.2");strcat(appstr,"x.xx");
    //    fprintf(stderr, DEPMASK2,appstr,"Compiler",Unp->DepAdr);
    //    break;
    //    case _I_DTL64   :
    //    fprintf(stderr, DEPMASK2,"DTL-64","Compiler",Unp->DepAdr);
    //    break;
    //    case _I_LASERBAS:
    //    fprintf(stderr, DEPMASK2,"LaserBasic","Compiler",Unp->DepAdr);
    //    break;
    //    case _I_HYPRA   :
    //    fprintf(stderr, DEPMASK2,"Hypra","Compiler",Unp->DepAdr);
    //    break;
    //    case _I_ISEPICLD:
    //    fprintf(stderr, DEPMASK2,"IsePic","Loader",Unp->DepAdr);
    //    break;
    //    case _I_NSUPACK10:
    //    sprintf(appstr,"%s %s","NSU","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_NSUPACK11:
    //    sprintf(appstr,"%s %s","NSU","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.1",Unp->DepAdr);
    //    break;
    //    case _I_ONSPACK:
    //    fprintf(stderr, DEPMASK2,"ONS","Packer",Unp->DepAdr);
    //    break;
    //    case _I_OPTIMUS:
    //    fprintf(stderr, DEPMASK2,"Optimus","Packer",Unp->DepAdr);
    //    break;
    //    case _I_OPTIMH:
    //    fprintf(stderr, DEPMASK2,"Optimus","Hack",Unp->DepAdr);
    //    break;
    //    case _I_OUGCOD:
    //    fprintf(stderr, DEPMASK2,"OUG","Coder",Unp->DepAdr);
    //    break;
    //    case _I_P100:
    //    fprintf(stderr, DEPMASK2,"P100","Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_PAKOPT:
    //    fprintf(stderr, DEPMASK2,"Packer","Optimizer",Unp->DepAdr);
    //    break;
    //    case _I_PITCOD:
    //    fprintf(stderr, DEPMASK2,"PIT","Coder",Unp->DepAdr);
    //    break;
    //    case _I_POLONCB:
    //    fprintf(stderr, DEPMASK2,"Polonus","Charblaster",Unp->DepAdr);
    //    break;
    //    case _I_POLONKR:
    //    sprintf(appstr,"%s %s","Krejzi","Packer");
    //    fprintf(stderr, DEPMASK2,"Polonus",appstr,Unp->DepAdr);
    //    break;
    //    case _I_POLONAM:
    //    sprintf(appstr,"%s %s","C64/+4/Amiga","Packer");
    //    fprintf(stderr, DEPMASK2,"Polonus",appstr,Unp->DepAdr);
    //    break;
    //    case _I_POLONEQ:
    //    fprintf(stderr, DEPMASK2,"Polonus","/ Equal chars",Unp->DepAdr);
    //    break;
    //    case _I_POLON4P:
    //    sprintf(appstr,"%s %s","$45c","Packer");
    //    fprintf(stderr, DEPMASK2,"Polonus",appstr,Unp->DepAdr);
    //    break;
    //    case _I_POLON101:
    //    sprintf(appstr,"%s %s","$101","Packer");
    //    fprintf(stderr, DEPMASK2,"Polonus",appstr,Unp->DepAdr);
    //    break;
    //    case _I_QRTPROT:
    //    fprintf(stderr, DEPMASK2,"Quartet","Protector",Unp->DepAdr);
    //    break;
  case _I_PUCRUNCH:
    fprintf(stderr, DEPMASK, "PUCrunch", Unp->DepAdr);
    break;
  case _I_PUCRUNCW:
    fprintf(stderr, DEPMASK2, "PUCrunch", "/ wrap", Unp->DepAdr);
    break;
  case _I_PUCRUNCS:
    fprintf(stderr, DEPMASK2, "PUCrunch", "/ short", Unp->DepAdr);
    break;
  case _I_PUCRUNCO:
    fprintf(stderr, DEPMASK2, "PUCrunch", "/ old?", Unp->DepAdr);
    break;
  case _I_PUCRUNCG:
    fprintf(stderr, DEPMASK2, "PUCrunch", "Generic Hack", Unp->DepAdr);
    break;
    //    case _I_PZWCOM:
    //    fprintf(stderr, DEPMASK2,"PZW","Compactor",Unp->DepAdr);
    //    break;
    //    case _I_RAPEQC:
    //    fprintf(stderr, DEPMASK2,"Rap","/ Equal chars",Unp->DepAdr);
    //    break;
  case _I_RLX3B:
    fprintf(stderr, DEPMASK2, "Relax", "3-byter", Unp->DepAdr);
    break;
  case _I_RLXP2:
    fprintf(stderr, DEPMASK2, "Relax Packer", "v2.x", Unp->DepAdr);
    break;
    //    case _I_SCRCR4:
    //    sprintf(appstr,"%s / %s","ScreenCrunch","2064");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_SCRCR6:
    //    sprintf(appstr,"%s / %s","ScreenCrunch","2066");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_SCRCRFCG:
    //    sprintf(appstr,"%s / %s","ScreenCrunch","FCG");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
  case _I_S8PACK:
    fprintf(stderr, DEPMASK2, "Section8", "Packer", Unp->DepAdr);
    break;
    //    case _I_CRMPACK:
    //    fprintf(stderr, DEPMASK2,"CRM","Packer",Unp->DepAdr);
    //    break;
    //    case _I_SHRPACK:
    //    fprintf(stderr, DEPMASK2,"Shurigen","Packer",Unp->DepAdr);
    //    break;
    //    case _I_SIRMPACK:
    //    strcpy(appstr,"Mini");strcat(appstr,"Packer");
    //    fprintf(stderr, DEPMASK2,"SIR",appstr,Unp->DepAdr);
    //    break;
    //    case _I_SIRMLINK:
    //    strcpy(appstr,"Master");strcat(appstr,"Linker");
    //    fprintf(stderr, DEPMASK2,"SIR",appstr,Unp->DepAdr);
    //    break;
    //    case _I_SIRCOMBIN1:
    //    sprintf(appstr,"%s %s","SIR","Combine");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_SIRCOMBIN2:
    //    sprintf(appstr,"%s %s","SIR","Combine");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_SIRCOMBIN3:
    //    sprintf(appstr,"%s %s","SIR","Combine");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.x",Unp->DepAdr);
    //    break;
    //    case _I_SKLCOD:
    //    fprintf(stderr, DEPMASK2,"Skylight","Protector",Unp->DepAdr);
    //    break;
    //    case _I_SLEDGE10:
    //    fprintf(stderr, DEPMASK2,"SledgeHammer","v1.0",Unp->DepAdr);
    //    break;
    //    case _I_SLEDGE11:
    //    fprintf(stderr, DEPMASK2,"SledgeHammer","v1.1",Unp->DepAdr);
    //    break;
    //    case _I_SLEDGE12:
    //    fprintf(stderr, DEPMASK2,"SledgeHammer","v1.2",Unp->DepAdr);
    //    break;
    //    case _I_KREATOR:
    //    strcpy(appstr,"v1.0");
    //    strcat(appstr,"/");
    //    strcat(appstr,"Kreator");
    //    fprintf(stderr, DEPMASK2,"SledgeHammer",appstr,Unp->DepAdr);
    //    break;
    //    case _I_SLEDGE20:
    //    fprintf(stderr, DEPMASK2,"SledgeHammer","v2.0",Unp->DepAdr);
    //    break;
    //    case _I_SLEDGE21:
    //    fprintf(stderr, DEPMASK2,"SledgeHammer","v2.1",Unp->DepAdr);
    //    break;
    //    case _I_SLEDGE23:
    //    fprintf(stderr, DEPMASK2,"SledgeHammer","v2.3",Unp->DepAdr);
    //    break;
    //    case _I_SLEDGE24:
    //    fprintf(stderr, DEPMASK2,"SledgeHammer","v2.4",Unp->DepAdr);
    //    break;
    //    case _I_SLEDGE2C:
    //    strcpy(appstr,"v2.2");
    //    strcat(appstr,"/");
    //    strcat(appstr,"CCS");
    //    fprintf(stderr, DEPMASK2,"SledgeHammer",appstr,Unp->DepAdr);
    //    break;
    //    case _I_SLEDGETRAP:
    //    strcpy(appstr,"v2.x");
    //    strcat(appstr,"/");
    //    strcat(appstr,"Trap");
    //    fprintf(stderr, DEPMASK2,"SledgeHammer",appstr,Unp->DepAdr);
    //    break;
    //    case _I_SLEDGE22:
    //    fprintf(stderr, DEPMASK2,"SledgeHammer","v2.2",Unp->DepAdr);
    //    break;
    //    case _I_SLEDGE30:
    //    fprintf(stderr, DEPMASK2,"SledgeHammer","v3.0",Unp->DepAdr);
    //    break;
    //    case _I_ULTIM8SH:
    //    fprintf(stderr, DEPMASK2,"Ultimate","SledgeHammer",Unp->DepAdr);
    //    break;
    //    case _I_SLEDGE1DP:
    //    sprintf(appstr,"%s %s","Packer","v2");
    //    fprintf(stderr, DEPMASK2,"Dreamr",appstr,Unp->DepAdr);
    //    break;
    //    case _I_MWSPCOMP:
    //    sprintf(appstr,"%s%s","Speedi","Compactor");
    //    fprintf(stderr, DEPMASK2,"Winterberg",appstr,Unp->DepAdr);
    //    break;
    //    case _I_MWPACK:
    //    fprintf(stderr, DEPMASK2,"Winterberg","Packer",Unp->DepAdr);
    //    break;
    //    case _I_MWSPCR1:
    //    sprintf(appstr,"%s%s","Speedi","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"MW1",Unp->DepAdr);
    //    break;
    //    case _I_MWSPCR2:
    //    sprintf(appstr,"%s%s","Speedi","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"MW2",Unp->DepAdr);
    //    break;
    //    case _I_SUPCRUNCH:
    //    sprintf(appstr,"%s%s","Super","Cruncher");appstr[strlen(appstr)-2]=0;
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
  case _I_TBCMULTI:
    fprintf(stderr, DEPMASK, "TBC Multicompactor", Unp->DepAdr);
    break;
  case _I_TBCMULT2:
    fprintf(stderr, DEPMASK2, "TBC Multicompactor", "v2.x", Unp->DepAdr);
    break;
  case _I_TBCMULTMOW:
    fprintf(stderr, DEPMASK2, "TBC Multicompactor", "AutoBreakSystem / Manowar", Unp->DepAdr);
    break;
    //    case _I_TCDLC1:
    //    fprintf(stderr, DEPMASK2,"TCD Link&Crunch","v1.x",Unp->DepAdr);
    //    break;
    //    case _I_TCDLC2:
    //    fprintf(stderr, DEPMASK2,"TCD Link&Crunch","v2.x",Unp->DepAdr);
    //    break;
    //    case _I_TCSCR2:
    //    sprintf(appstr,"%s %s","TCS","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_TCSCR3:
    //    sprintf(appstr,"%s %s","TCS","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.0",Unp->DepAdr);
    //    break;
    //    case _I_TC40   :
    //    fprintf(stderr, DEPMASK2,"Time Cruncher","v4.0",Unp->DepAdr);
    //    break;
    //    case _I_TC42H  :
    //    sprintf(appstr,"%s / %s","v4.2","header");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC40HIL:
    //    sprintf(appstr,"%s / %s","v4.2","header");
    //    strcat(appstr," ");strcat(appstr,"ILS");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TCHVIK:
    //    sprintf(appstr,"%s %s","Vikings","header");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC42   :
    //    fprintf(stderr, DEPMASK2,"Time Cruncher","v4.2",Unp->DepAdr);
    //    break;
    //    case _I_TC43   :
    //    fprintf(stderr, DEPMASK2,"Time Cruncher","v4.3",Unp->DepAdr);
    //    break;
    //    case _I_TC44   :
    //    fprintf(stderr, DEPMASK2,"Time Cruncher","v4.4",Unp->DepAdr);
    //    break;
    //    case _I_TC50   :
    //    fprintf(stderr, DEPMASK2,"Time Cruncher","v5.0",Unp->DepAdr);
    //    break;
    //    case _I_TC51   :
    //    fprintf(stderr, DEPMASK2,"Time Cruncher","v5.1",Unp->DepAdr);
    //    break;
    //    case _I_TC52   :
    //    fprintf(stderr, DEPMASK2,"Time Cruncher","v5.2",Unp->DepAdr);
    //    break;
    //    case _I_TC53   :
    //    fprintf(stderr, DEPMASK2,"Time Cruncher","v5.3",Unp->DepAdr);
    //    break;
    //    case _I_TC5GEN:
    //    sprintf(appstr,"%s / %s","v5.x","Generic Hack");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC30   :
    //    fprintf(stderr, DEPMASK2,"Time Cruncher","v3.0",Unp->DepAdr);
    //    break;
    //    case _I_TC31   :
    //    fprintf(stderr, DEPMASK2,"Time Cruncher","v3.1",Unp->DepAdr);
    //    break;
    //    case _I_TC33   :
    //    fprintf(stderr, DEPMASK2,"Time Cruncher","v3.3",Unp->DepAdr);
    //    break;
    //    case _I_TC2MHZ1:
    //    sprintf(appstr,"%s %s","2MHZ","v1.0");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC2MHZ2:
    //    sprintf(appstr,"%s %s","2MHZ","v2.0");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC32061:
    //    sprintf(appstr,"%s / %s","v3.x","2061");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TCMC4  :
    //    fprintf(stderr, DEPMASK2,"Time Cruncher","/
    //    MegaCrunch 4.0",Unp->DepAdr); break; case _I_TC3RWE :
    //    sprintf(appstr,"%s / %s","v3.x","RWE");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC32072:
    //    sprintf(appstr,"%s / %s","v3.x","2072");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3RAD :
    //    sprintf(appstr,"%s / %s","v3.x","Radical");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC32074:
    //    sprintf(appstr,"%s / %s","v3.x","2074");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3FPE :
    //    sprintf(appstr,"%s / %s","v3.x","Fast Pack'Em ");
    //    strcat(appstr,"v2.1");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3FBI :
    //    sprintf(appstr,"%s / %s","v3.x","FBI");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3TRI :
    //    sprintf(appstr,"%s / %s","v3.x","Triumwyrat");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3HTL :
    //    sprintf(appstr,"%s / %s","v3.x","HTL");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3ENT :
    //    sprintf(appstr,"%s / %s","v3.x","Entropy");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TCGENH :
    //    sprintf(appstr,"%s / %s","Time Cruncher","Generic Hack");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_TCFPEX :
    //    fprintf(stderr, DEPMASK,"File Press Expert",Unp->DepAdr);
    //    break;
    //    case _I_TCSIR1 :
    //    fprintf(stderr, DEPMASK2,"SIR Compact","v1.x",Unp->DepAdr);
    //    break;
    //    case _I_TCUNKH :
    //    fprintf(stderr, DEPMASK2,"Unknown","TC hack",Unp->DepAdr);
    //    break;
    //    case _I_TCSIR4 :
    //    fprintf(stderr, DEPMASK2,"SIR Compact","v4.x",Unp->DepAdr);
    //    break;
    //    case _I_TCSIR3 :
    //    fprintf(stderr, DEPMASK2,"SIR Compact","v3.x",Unp->DepAdr);
    //    break;
    //    case _I_TCSIR2 :
    //    fprintf(stderr, DEPMASK2,"SIR Compact","v2.x",Unp->DepAdr);
    //    break;
    //    case _I_TC5SC  :
    //    sprintf(appstr,"%s / %s","v5.x","SupraCompactor");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3RLX :
    //    sprintf(appstr,"%s / %s","v3.x","Relax");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC33AD :
    //    sprintf(appstr,"%s / %s","v3.x","Triad");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3PC :
    //    sprintf(appstr,"%s / %s","v3.x","PC");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC423AD:
    //    sprintf(appstr,"%s / %s","v4.2","Triad");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC42GEN:
    //    sprintf(appstr,"%s / %s","v4.2","Generic Hack");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3TDF :
    //    sprintf(appstr,"%s / %s","v3.x","TDF");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3ATC :
    //    sprintf(appstr,"%s / %s","v3.x","ATC");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3F4CG:
    //    sprintf(appstr,"%s / %s","v3.x","F4CG");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3HSCG:
    //    sprintf(appstr,"%s / %s","v3.x","HSCG");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3HSFE:
    //    sprintf(appstr,"%s / %s","v3.x","HSCG");strcat(appstr,"+FE");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3TTN :
    //    sprintf(appstr,"%s / %s","v3.x","Triton");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3MULE:
    //    sprintf(appstr,"%s / %s","v3.x","Mule");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3AGILE:
    //    sprintf(appstr,"%s / %s","v3.x","Agile");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3S451:
    //    sprintf(appstr,"%s / %s","v3.1","S451");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3S451V2:
    //    sprintf(appstr,"%s / %s","v3.2","S451");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3IKARI:
    //    sprintf(appstr,"%s / %s","v3.x","Ikari");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TC3U   :
    //    sprintf(appstr,"%s / %s","v3.x","Unknown");
    //    fprintf(stderr, DEPMASK2,"Time Cruncher",appstr,Unp->DepAdr);
    //    break;
    //    case _I_TMCCOD :
    //    fprintf(stderr, DEPMASK2,"TMC","Coder",Unp->DepAdr);
    //    break;
    //    case _I_TMMPACK:
    //    fprintf(stderr, DEPMASK2,"TMM","Packer",Unp->DepAdr);
    //    break;
    //    case _I_TRASHC1:
    //    fprintf(stderr, DEPMASK2,"Trashcan","v1.x",Unp->DepAdr);
    //    break;
    //    case _I_TRASHC2:
    //    fprintf(stderr, DEPMASK2,"Trashcan","v2.x",Unp->DepAdr);
    //    break;
    //    case _I_TRASHCU:
    //    fprintf(stderr, DEPMASK2,"Trashcan","Unknown",Unp->DepAdr);
    //    break;
    //    case _I_TRIANP:
    //    fprintf(stderr, DEPMASK2,"Trianon","Packer",Unp->DepAdr);
    //    break;
    //    case _I_TRIAN2:
    //    fprintf(stderr, DEPMASK2,"Trianon","v2.x",Unp->DepAdr);
    //    break;
    //    case _I_TRIAN3:
    //    fprintf(stderr, DEPMASK2,"Trianon","v3.x",Unp->DepAdr);
    //    break;
    //    case _I_TRIANC:
    //    fprintf(stderr, DEPMASK2,"Trianon","Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_TSBP1:
    //    sprintf(appstr,"%s %s","TSB","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_TSBP21:
    //    sprintf(appstr,"%s %s","TSB","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.1",Unp->DepAdr);
    //    break;
    //    case _I_TSBP3:
    //    sprintf(appstr,"%s %s","TSB","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.x",Unp->DepAdr);
    //    break;
    //    case _I_TSMCOD:
    //    fprintf(stderr, DEPMASK2,"TSM","Coder",Unp->DepAdr);
    //    break;
    //    case _I_TSMPACK:
    //    fprintf(stderr, DEPMASK2,"TSM","Packer",Unp->DepAdr);
    //    break;
    //    case _I_TURBOP:
    //    fprintf(stderr, DEPMASK2,"Turbo","Packer",Unp->DepAdr);
    //    break;
    //    case _I_ULTRAC3:
    //    fprintf(stderr, DEPMASK2,"UltraComp","v3.0",Unp->DepAdr);
    //    break;
    //    case _I_UNIPACK1:
    //    sprintf(appstr,"%s%s","Uni","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_UNIPACK2:
    //    sprintf(appstr,"%s%s","Uni","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_UNIPACK3:
    //    sprintf(appstr,"%s%s","Uni","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.x",Unp->DepAdr);
    //    break;
    //    case _I_U_100_P2:
    //    sprintf(appstr,"%s %s","$100","Packer");
    //    strcat(appstr," ");strcat(appstr,"v2.x");
    //    fprintf(stderr, DEPMASK2,"Unknown",appstr,Unp->DepAdr);
    //    break;
    //    case _I_U_100_P3:
    //    sprintf(appstr,"%s %s","$100","Packer");
    //    strcat(appstr," ");strcat(appstr,"v3.x");
    //    fprintf(stderr, DEPMASK2,"Unknown",appstr,Unp->DepAdr);
    //    break;
    //    case _I_U_100_P4:
    //    sprintf(appstr,"%s %s","$100","Packer");
    //    strcat(appstr," ");strcat(appstr,"v4.x");
    //    fprintf(stderr, DEPMASK2,"Unknown",appstr,Unp->DepAdr);
    //    break;
    //    case _I_U_100_P5:
    //    sprintf(appstr,"%s %s","$100","Packer");
    //    strcat(appstr," ");strcat(appstr,"v5.x");
    //    fprintf(stderr, DEPMASK2,"Unknown",appstr,Unp->DepAdr);
    //    break;
    //    case _I_U_101:
    //    sprintf(appstr,"%s %s","$101","Packer");
    //    fprintf(stderr, DEPMASK2,"Unknown",appstr,Unp->DepAdr);
    //    break;
    //    case _I_U_111:
    //    sprintf(appstr,"%s %s","$111","Packer");
    //    fprintf(stderr, DEPMASK2,"Unknown",appstr,Unp->DepAdr);
    //    break;
    //    case _I_ROWS:
    //    fprintf(stderr, DEPMASK2,"Rows","Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_FALCOP:
    //    fprintf(stderr, DEPMASK2,"Falco Paul","Packer",Unp->DepAdr);
    //    break;
    //    case _I_PSQLINK1:
    //    sprintf(appstr,"%s %s","PSQ","Linker");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.1",Unp->DepAdr);
    //    break;
    //    case _I_U_439:
    //    sprintf(appstr,"%s %s","$439","Packer");
    //    fprintf(stderr, DEPMASK2,"Unknown",appstr,Unp->DepAdr);
    //    break;
    //    case _I_ENTROPYPK:
    //    fprintf(stderr, DEPMASK2,"Entropy","Packer",Unp->DepAdr);
    //    break;
    //    case _I_U_8E:
    //    sprintf(appstr,"%s %s","$8e","Packer");
    //    fprintf(stderr, DEPMASK2,"Unknown",appstr,Unp->DepAdr);
    //    break;
    //    case _I_U_25:
    //    sprintf(appstr,"%s %s","$25","Packer");
    //    fprintf(stderr, DEPMASK2,"Unknown",appstr,Unp->DepAdr);
    //    break;
    //    case _I_DSCTITMK:
    //    fprintf(stderr, DEPMASK2,"DSCompware","Super-TitleMaker",Unp->DepAdr);
    //    break;
    //    case _I_DSCCR:
    //    fprintf(stderr, DEPMASK2,"DSCompware","Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_DSCPK:
    //    fprintf(stderr, DEPMASK2,"DSCompware","Packer",Unp->DepAdr);
    //    break;
    //    case _I_DSCPK2:
    //    sprintf(appstr,"%s %s","DSCompware","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_KRESS:
    //    fprintf(stderr, DEPMASK2,"Kress","Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_U_1WF2:
    //    sprintf(appstr,"%s %s","$f2","Packer");
    //    fprintf(stderr, DEPMASK2,"Oneway",appstr,Unp->DepAdr);
    //    break;
    //    case _I_U_1WDA:
    //    sprintf(appstr,"%s %s","$da","Packer");
    //    fprintf(stderr, DEPMASK2,"Oneway",appstr,Unp->DepAdr);
    //    break;
    //    case _I_GENSYSL:
    //    fprintf(stderr, "Generic SYS-less %s $%04x\n","Packer",Unp->Forced);
    //    break;
    //    case _I_U_P3:
    //    fprintf(stderr, DEPMASK2,"Unknown","P3",Unp->DepAdr);
    //    break;
    //    case _I_U_3ADCR:
    //    sprintf(appstr,"%s %s","Unknown","Triad");
    //    fprintf(stderr, DEPMASK2,appstr,"Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_VIP1X:
    //    fprintf(stderr, DEPMASK2,"VIP","v1.x",Unp->DepAdr);
    //    break;
    //    case _I_VIP20:
    //    fprintf(stderr, DEPMASK2,"VIP","v2.0",Unp->DepAdr);
    //    break;
    //    case _I_TOTP1X:
    //    sprintf(appstr,"%s %s","Total","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_TOTP20:
    //    sprintf(appstr,"%s %s","Total","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_VIP3X:
    //    fprintf(stderr, DEPMASK2,"VIP","v3.x",Unp->DepAdr);
    //    break;
    //    case _I_HIT02:
    //    sprintf(appstr,"%s %s","HitMen","$02");
    //    fprintf(stderr, DEPMASK2,appstr,"Packer",Unp->DepAdr);
    //    break;
    //    case _I_HIT20:
    //    sprintf(appstr,"%s %s","HitMen","$20");
    //    fprintf(stderr, DEPMASK2,appstr,"Linker",Unp->DepAdr);
    //    break;
    //    case _I_AGNUS02:
    //    sprintf(appstr,"%s %s","Agnus","$02");
    //    fprintf(stderr, DEPMASK2,appstr,"Packer",Unp->DepAdr);
    //    break;
    //    case _I_BUK02:
    //    sprintf(appstr,"%s %s","Buraki","$02");
    //    fprintf(stderr, DEPMASK2,appstr,"Packer",Unp->DepAdr);
    //    break;
    //    case _I_WARLOCK:
    //    fprintf(stderr, DEPMASK2,"Warlock","Packer",Unp->DepAdr);
    //    break;
    //    case _I_WCCMC:
    //    fprintf(stderr, DEPMASK2,"WCC","Modem Converter",Unp->DepAdr);
    //    break;
    //    case _I_WDRPROT:
    //    sprintf(appstr,"%s %s","Soft","Protector");
    //    fprintf(stderr, DEPMASK2,"WDR",appstr,Unp->DepAdr);
    //    break;
    //    case _I_WDRLAMEK:
    //    sprintf(appstr,"%s %s","Lamer","Killer");
    //    fprintf(stderr, DEPMASK2,"WDR",appstr,Unp->DepAdr);
    //    break;
    //    case _I_WGICOD:
    //    fprintf(stderr, DEPMASK2,"WGI","Coder",Unp->DepAdr);
    //    break;
    //    case _I_3CZIP:
    //    fprintf(stderr, DEPMASK2,"3Code","Zipper",Unp->DepAdr);
    //    break;
    //    case _I_4CZIP:
    //    sprintf(appstr,"%s %s","Zipper","v2.0");
    //    fprintf(stderr, DEPMASK2,"4Code",appstr,Unp->DepAdr);
    //    break;
    //    case _I_4CZIP2S:
    //    sprintf(appstr,"%s %s","Zipper","v2.S");
    //    fprintf(stderr, DEPMASK2,"4Code",appstr,Unp->DepAdr);
    //    break;
    //    case _I_XTERM:
    //    sprintf(appstr,"%s %s","X-Terminator","v2.0");strcat(appstr," /");
    //    fprintf(stderr, DEPMASK2,appstr,"FLT",Unp->DepAdr);
    //    break;
    //    case _I_ILSCOMP3X:
    //    sprintf(appstr,"%s %s","ILS","Compacker");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.x",Unp->DepAdr);
    //    break;
    //    case _I_ILSCOMP2X:
    //    sprintf(appstr,"%s %s","ILS","Compacker");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_3CZIP2:
    //    sprintf(appstr,"%s %s","Zipper","v2.0");
    //    fprintf(stderr, DEPMASK2,"3Code",appstr,Unp->DepAdr);
    //    break;
    //    case _I_XDSV1:
    //    sprintf(appstr,"%s %s","XDS","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_XDSV2:
    //    sprintf(appstr,"%s %s","XDS","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_XDSV3:
    //    sprintf(appstr,"%s %s","XDS","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.0",Unp->DepAdr);
    //    break;
    //    case _I_XDSK1:
    //    sprintf(appstr,"%s %s","XDS","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"K1",Unp->DepAdr);
    //    break;break;
    //    case _I_XDSK2:
    //    sprintf(appstr,"%s %s","XDS","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"K2",Unp->DepAdr);
    //    break;
    //    case _I_XDSK3:
    //    sprintf(appstr,"%s %s","XDS","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"K3",Unp->DepAdr);
    //    break;
    //    case _I_XIP:
    //    fprintf(stderr, DEPMASK,"XIP",Unp->DepAdr);
    //    break;
        case _I_XTC10:
        fprintf(stderr, DEPMASK2,"XTC Packer","v1.0",Unp->DepAdr);
        break;
        case _I_XTC21:
        fprintf(stderr, DEPMASK2,"XTC Packer","v2.1",Unp->DepAdr);
        break;
        case _I_XTC22:
        fprintf(stderr, DEPMASK2,"XTC Packer","v2.2",Unp->DepAdr);
        break;
        case _I_XTC23:
        fprintf(stderr, DEPMASK2,"XTC Packer","v2.3",Unp->DepAdr);
        break;
        case _I_XTC23GP:
        fprintf(stderr, DEPMASK2,"XTC Packer v2.3 /","G*P",Unp->DepAdr);
        break;
        case _I_XTC2XGP:
        fprintf(stderr, DEPMASK2,"XTC Packer v2.x /","G*P",Unp->DepAdr);
        break;
    //    case _I_ZIGAG:
    //    fprintf(stderr, DEPMASK2,"Zigag","Packer",Unp->DepAdr);
    //    break;
    //    case _I_ZIP50C:
    //    sprintf(appstr,"%s%s","v5.0","/Chromizer");
    //    fprintf(stderr, DEPMASK2,"Zipper",appstr,Unp->DepAdr);
    //    break;
    //    case _I_ZIP50H:
    //    sprintf(appstr,"%s %s","v5.0","Hack");
    //    fprintf(stderr, DEPMASK2,"Zipper",appstr,Unp->DepAdr);
    //    break;
    //    case _I_ZIP511:
    //    sprintf(appstr,"%s%s","v5.1","/1");
    //    fprintf(stderr, DEPMASK2,"Zipper",appstr,Unp->DepAdr);
    //    break;
    //    case _I_ZIP512:
    //    sprintf(appstr,"%s%s","v5.1","/2");
    //    fprintf(stderr, DEPMASK2,"Zipper",appstr,Unp->DepAdr);
    //    break;
    //    case _I_ZIP513:
    //    sprintf(appstr,"%s%s","v5.1","/3");
    //    fprintf(stderr, DEPMASK2,"Zipper",appstr,Unp->DepAdr);
    //    break;
    //    case _I_ZIP51XEN:
    //    sprintf(appstr,"%s%s","v5.1","/Xenon");
    //    fprintf(stderr, DEPMASK2,"Zipper",appstr,Unp->DepAdr);
    //    break;
    //    case _I_ZIP3:
    //    fprintf(stderr, DEPMASK2,"Zipper","v3.0",Unp->DepAdr);
    //    break;
    //    case _I_ZIP8:
    //    fprintf(stderr, DEPMASK2,"Zipper","v8.x",Unp->DepAdr);
    //    break;
    //    case _I_STLPROT:
    //    fprintf(stderr, DEPMASK2,"STL","Protector",Unp->DepAdr);
    //    break;
    //    case _I_GRFBIN:
    //    fprintf(stderr, DEPMASK2,"GrafBinaer","Coder",Unp->DepAdr);
    //    break;
    //    case _I_ABYSSCOD1:
    //    sprintf(appstr,"%s %s","Abyss","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_ABYSSCOD2:
    //    sprintf(appstr,"%s %s","Abyss","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_SPCCOD:
    //    fprintf(stderr, DEPMASK2,"SPC","Coder",Unp->DepAdr);
    //    break;
    //    case _I_SPCPROT:
    //    fprintf(stderr, DEPMASK2,"SPC","Protector",Unp->DepAdr);
    //    break;
    //    case _I_COBRACOD:
    //    fprintf(stderr, DEPMASK2,"Cobra","Coder",Unp->DepAdr);
    //    break;
    //    case _I_FSWPACK1:
    //    sprintf(appstr,"%s %s","FSW","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_FSWPACK2:
    //    sprintf(appstr,"%s %s","FSW","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_FSWPACK3:
    //    sprintf(appstr,"%s %s","FSW","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.0",Unp->DepAdr);
    //    break;
    //    case _I_FSWPROT:
    //    fprintf(stderr, DEPMASK2,"FSW","Protector",Unp->DepAdr);
    //    break;
    //    case _I_BAMCOD1:
    //    fprintf(stderr, DEPMASK2,"BAM","Compacker",Unp->DepAdr);
    //    break;
    //    case _I_BAMCOD2:
    //    sprintf(appstr,"%s %s","BAM","Reductor");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.3",Unp->DepAdr);
    //    break;
    //    case _I_BAMCOD3:
    //    sprintf(appstr,"%s %s","BAM","Reductor");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.0",Unp->DepAdr);
    //    break;
    //    case _I_BYTEBUST4L:
    //    sprintf(appstr,"%s%s","Byte","Buster");
    //    fprintf(stderr, DEPMASK2,appstr,"/ Low",Unp->DepAdr);
    //    break;
    //    case _I_BYTEBUST4H:
    //    sprintf(appstr,"%s%s","Byte","Buster");
    //    fprintf(stderr, DEPMASK2,appstr,"/ High",Unp->DepAdr);
    //    break;
    //    case _I_MEKKER:
    //    fprintf(stderr, DEPMASK,"Mekker",Unp->DepAdr);
    //    break;
    //    case _I_BYTESTRN:
    //    sprintf(appstr,"%s%s","Byte","Strainer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_JEDILINK1:
    //    sprintf(appstr,"%s %s","Jedi","Linker");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_JEDILINK2:
    //    sprintf(appstr,"%s %s","Jedi","Linker");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_JEDIPACK:
    //    fprintf(stderr, DEPMASK2,"Jedi","Packer",Unp->DepAdr);
    //    break;
    //    case _I_PRIDE:
    //    fprintf(stderr, DEPMASK2,"Pride","Packer",Unp->DepAdr);
    //    break;
    //    case _I_STARCRUNCH:
    //    fprintf(stderr, DEPMASK,"StarCrunch",Unp->DepAdr);
    //    break;
    //    case _I_SPYPACK:
    //    fprintf(stderr, DEPMASK2,"Spy","Packer",Unp->DepAdr);
    //    break;
    //    case _I_PETPACK:
    //    fprintf(stderr, DEPMASK2,"PET","Packer",Unp->DepAdr);
    //    break;
    //    case _I_CADAVERPACK:
    //    fprintf(stderr, DEPMASK2,"Cadaver","Packer",Unp->DepAdr);
    //    break;
    //    case _I_HELMET2:
    //    fprintf(stderr, DEPMASK2,"Helmet","v2.x",Unp->DepAdr);
    //    break;
    //    case _I_ANTIRAM1:
    //    sprintf(appstr,"%s %s","Antiram","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_ANTIRAM2:
    //    sprintf(appstr,"%s %s","Antiram","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_ANTICOM:
    //    fprintf(stderr, DEPMASK2,"Anticom","Packer",Unp->DepAdr);
    //    break;
    //    case _I_GCSAUTO:
    //    fprintf(stderr, DEPMASK2,"GCS","Autostarter",Unp->DepAdr);
    //    break;
    //    case _I_AUTOFLG:
    //    fprintf(stderr, DEPMASK2,"AFLG/M.Pall","Autostarter",Unp->DepAdr);
    //    break;
    //    case _I_EXCPACK:
    //    fprintf(stderr, DEPMASK2,"Excalibur 666","Packer",Unp->DepAdr);
    //    break;
    //    case _I_EXCCOD:
    //    fprintf(stderr, DEPMASK2,"Excalibur 666","Coder",Unp->DepAdr);
    //    break;
    //    case _I_CNCD:
    //    sprintf(appstr,"%s %s","CNCD","Classic");
    //    fprintf(stderr, DEPMASK2,appstr,"Packer",Unp->DepAdr);
    //    break;
    //    case _I_4WD:
    //    fprintf(stderr, DEPMASK2,"4WD-Soft","Packer",Unp->DepAdr);
    //    break;
    //    case _I_1WMKL2:
    //    sprintf(appstr,"%s %s","Oneway","Makt-Link");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_EMUFUX1:
    //    sprintf(appstr,"%s %s","Plush","Emu-Fuxx0r");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_EMUFUX2:
    //    sprintf(appstr,"%s %s","Plush","Emu-Fuxx0r");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_HUFFER:
    //    sprintf(appstr,"%s / %s","v1.0","FLT");
    //    fprintf(stderr, DEPMASK2,"Huffer",appstr,Unp->DepAdr);
    //    break;
    //    case _I_HUFFERQC:
    //    sprintf(appstr,"%s / %s","v1.1","Quick");strcat(appstr,"Cruncher");
    //    fprintf(stderr, DEPMASK2,"Huffer",appstr,Unp->DepAdr);
    //    break;
    //    case _I_NM156PACK:
    //    fprintf(stderr, DEPMASK2,"NM156","Packer",Unp->DepAdr);
    //    break;
    //    case _I_NM156LINK:
    //    fprintf(stderr, DEPMASK2,"NM156","Linker",Unp->DepAdr);
    //    break;
    //    case _I_RATTPACK:
    //    fprintf(stderr, DEPMASK2,"Ratt","Packer",Unp->DepAdr);
    //    break;
    //    case _I_RATTLINK:
    //    fprintf(stderr, DEPMASK2,"Ratt","Linker",Unp->DepAdr);
    //    break;
    //    case _I_BYTERAPERS1:
    //    sprintf(appstr,"%s %s","Byterapers","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_BYTERAPERS2:
    //    sprintf(appstr,"%s %s","Byterapers","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_CIACRP:
    //    sprintf(appstr,"%s %s","CIA","Crypt");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_PANPACK:
    //    sprintf(appstr,"%s %s","PAN","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_PANPACK2:
    //    sprintf(appstr,"%s %s","PAN","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_TKCEQB1:
    //    sprintf(appstr,"%s %s","TKC","Bytepress");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_TKCEQB2:
    //    sprintf(appstr,"%s %s","TKC","Bytepress");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_UPROT420:
    //    sprintf(appstr,"%s %s","$420","Coder");
    //    fprintf(stderr, DEPMASK2,"Unknown",appstr,Unp->DepAdr);
    //    break;
    //    case _I_HIVIRUS:
    //    fprintf(stderr, DEPMASK2,"HIV","Virus",Unp->DepAdr);
    //    break;
    //    case _I_BHPVIRUS:
    //    fprintf(stderr, DEPMASK2,"BHP","Virus",Unp->DepAdr);
    //    break;
    //    case _I_BULAVIRUS:
    //    fprintf(stderr, DEPMASK2,"Bula","Virus",Unp->DepAdr);
    //    break;
    //    case _I_CODERVIRUS:
    //    fprintf(stderr, DEPMASK2,"Coder","Virus",Unp->DepAdr);
    //    break;
    //    case _I_BOAVIRUS:
    //    fprintf(stderr, DEPMASK2,"Boa","Virus",Unp->DepAdr);
    //    break;
    //    case _I_PLUSHZ:
    //    sprintf(appstr,"%s %s","Plush","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"/ fast",Unp->DepAdr);
    //    break;
    //    case _I_PLUSHZS:
    //    sprintf(appstr,"%s %s","Plush","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"/ short",Unp->DepAdr);
    //    break;
    //    case _I_REBELP:
    //    fprintf(stderr, DEPMASK2,"Rebels","Packer",Unp->DepAdr);
    //    break;
    //    case _I_YETICOD:
    //    fprintf(stderr, DEPMASK2,"Yeti","Coder",Unp->DepAdr);
    //    break;
    //    case _I_ZEROCOD:
    //    fprintf(stderr, DEPMASK2,"Zero","Coder",Unp->DepAdr);
    //    break;
    //    case _I_CHGPROT2:
    //    sprintf(appstr,"%s %s","Change","Protector");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_PDPACK1:
    //    sprintf(appstr,"%s %s","Panoramic","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_PDPACK2:
    //    sprintf(appstr,"%s %s","Panoramic","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.0",Unp->DepAdr);
    //    break;
    //    case _I_PDPACK30:
    //    sprintf(appstr,"%s %s","Panoramic","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.0",Unp->DepAdr);
    //    break;
    //    case _I_PDPACK31:
    //    sprintf(appstr,"%s %s","Panoramic","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.1",Unp->DepAdr);
    //    break;
    //    case _I_PDPACK32:
    //    sprintf(appstr,"%s %s","Panoramic","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v3.2",Unp->DepAdr);
    //    break;
    //    case _I_PDEQLZ15:
    //    sprintf(appstr,"%s %s","Panoramic","Equalizer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.5",Unp->DepAdr);
    //    break;
    //    case _I_SUPERPACK4:
    //    sprintf(appstr,"%s%s","Super","Packer");appstr[strlen(appstr)-2]=0;
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_THSCOD:
    //    sprintf(appstr,"%s%s","Bit","Coder");
    //    fprintf(stderr, DEPMASK2,"THS",appstr,Unp->DepAdr);
    //    break;
    //    case _I_PWCOPSH:
    //    sprintf(appstr,"%s %s","Password","Protector");strcat(appstr,":");
    //    fprintf(stderr, DEPMASK2,appstr,"Cop Shocker",Unp->DepAdr);
    //    break;
    //    case _I_PWTIMEC:
    //    sprintf(appstr,"%s %s","Password","Protector");strcat(appstr,":");
    //    fprintf(stderr, DEPMASK2,appstr,"Time Coder/Trap",Unp->DepAdr);
    //    break;
    //    case _I_PWSNACKY:
    //    sprintf(appstr,"%s %s","Password","Protector");strcat(appstr,":");
    //    fprintf(stderr, DEPMASK2,appstr,"Snacky",Unp->DepAdr);
    //    break;
    //    case _I_PWTCD:
    //    sprintf(appstr,"%s %s","Password","Protector");strcat(appstr,":");
    //    fprintf(stderr, DEPMASK2,appstr,"TCD",Unp->DepAdr);
    //    break;
    //    case _I_PWJCH:
    //    sprintf(appstr,"%s %s","Password","Protector");strcat(appstr,":");
    //    fprintf(stderr, DEPMASK2,appstr,"J-Coder v1.0/JCH",Unp->DepAdr);
    //    break;
    //    case _I_PWFILEPR:
    //    sprintf(appstr,"%s %s","Password","Protector");strcat(appstr,":");
    //    fprintf(stderr, DEPMASK2,appstr,"FileProtect
    //    v1.0/Syncro",Unp->DepAdr); break; case _I_PWFLT: sprintf(appstr,"%s
    //    %s","Password","Protector");strcat(appstr,":"); fprintf(stderr,
    //    DEPMASK2,appstr,"Lamer-Protection/FLT",Unp->DepAdr); break; case
    //    _I_WHO: fprintf(stderr, DEPMASK2,"WHO","Coder",Unp->DepAdr); break;
    //    case _I_PYRACOD1:
    //    sprintf(appstr,"%s %s","Pyra","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_PYRACOD2:
    //    sprintf(appstr,"%s %s","Pyra","Coder");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_BFPACK:
    //    fprintf(stderr, DEPMASK2,"BeyondForce","Packer",Unp->DepAdr);
    //    break;
    //    case _I_THUNCATSPK:
    //    fprintf(stderr, DEPMASK2,"ThunderCats","Packer",Unp->DepAdr);
    //    break;
  case _I_ACTIONP:
    fprintf(stderr, DEPMASK2, "Action", "Packer", Unp->DepAdr);
    break;
    //    case _I_KGBCOD:
    //    fprintf(stderr, DEPMASK2,"KGB","Coder",Unp->DepAdr);
    //    break;
    //    case _I_CCRYPT:
    //    fprintf(stderr, DEPMASK2,"C+C","Coder",Unp->DepAdr);
    //    break;
    //    case _I_TDT:
    //    fprintf(stderr, DEPMASK2,"DreamTeam","Packer",Unp->DepAdr);
    //    break;
    //    case _I_TDTPETC1:
    //    sprintf(appstr,"%s %s","PET","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_TDTPETC2:
    //    sprintf(appstr,"%s %s","PET","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.1",Unp->DepAdr);
    //    break;
    //    case _I_TDTPETCU:
    //    sprintf(appstr,"%s %s","PET","Cruncher");
    //    fprintf(stderr, DEPMASK2,appstr,"Unknown",Unp->DepAdr);
    //    break;
    //    case _I_TDTPETL:
    //    fprintf(stderr, DEPMASK2,"PET","Linker",Unp->DepAdr);
    //    break;
    //    case _I_CULT:
    //    fprintf(stderr, DEPMASK2,"Cult","Packer",Unp->DepAdr);
    //    break;
    //    case _I_GIMZO:
    //    fprintf(stderr, DEPMASK2,"Gimzo","Packer",Unp->DepAdr);
    //    break;
    //    case _I_BETASKIP:
    //    fprintf(stderr, DEPMASK2,"BetasSkip","Autostarter",Unp->DepAdr);
    //    break;
    //    case _I_BLACKADDER:
    //    fprintf(stderr, DEPMASK2,"BlackAdder","header",Unp->DepAdr);
    //    break;
    //    case _I_ICS8:
    //    fprintf(stderr, DEPMASK2,"ICS Drive 8","Coder",Unp->DepAdr);
    //    break;
    //    case _I_PARADROID1:
    //    sprintf(appstr,"%s %s","Paradroid","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.x",Unp->DepAdr);
    //    break;
    //    case _I_PARADROID2:
    //    sprintf(appstr,"%s %s","Paradroid","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v2.x",Unp->DepAdr);
    //    break;
    //    case _I_PARADROIDL:
    //    fprintf(stderr, DEPMASK2,"Paradroid","Linker",Unp->DepAdr);
    //    break;
    //    case _I_PLASMACOD:
    //    fprintf(stderr, DEPMASK2,"Plasma","Coder",Unp->DepAdr);
    //    break;
    //    case _I_EXACTCOD:
    //    fprintf(stderr, DEPMASK2,"Exact","Coder",Unp->DepAdr);
    //    break;
    //    case _I_DRZOOM:
    //    sprintf(appstr,"%s %s","Dr Zoom","Packer");
    //    fprintf(stderr, DEPMASK2,appstr,"v1.0",Unp->DepAdr);
    //    break;
    //    case _I_TIMCR:
    //    strcpy(appstr,"Cruncher");appstr[6]=0;
    //    fprintf(stderr, DEPMASK2,"Tim",appstr,Unp->DepAdr);
    //    break;
    //    case _I_CNETFIX:
    //    fprintf(stderr, DEPMASK2,"CNET Fixer","Coder",Unp->DepAdr);
    //    break;
    //    case _I_TRIANGLECOD:
    //    fprintf(stderr, DEPMASK2,"Triangle","Coder",Unp->DepAdr);
    //    break;
    //    case _I_SFLINK:
    //    fprintf(stderr, DEPMASK2,"Super File","Linker",Unp->DepAdr);
    //    break;
    //    case _I_BONGO:
    //    fprintf(stderr, DEPMASK2,"Bongo","Cruncher",Unp->DepAdr);
    //    break;
    //    case _I_JEMABASIC:
    //    sprintf(appstr,"%s %s","Jemasoft","Basic");
    //    fprintf(stderr, DEPMASK2,appstr,"Packer",Unp->DepAdr);
    //    break;
    //    case _I_TATCOD:
    //    fprintf(stderr, DEPMASK2,"TAT","Coder",Unp->DepAdr);
    //    break;
    //    case _I_RFOCOD:
    //    fprintf(stderr, DEPMASK2,"RFO","Coder",Unp->DepAdr);
    //    break;
    //    case _I_DOYNAMITE:
    //    fprintf(stderr, DEPMASK2,"Doynamite","v1.1",Unp->DepAdr);
    //    break;
    //    case _I_BITNAX:
    //    fprintf(stderr, DEPMASK2,"Doynamite/Bitnax","v1.0",Unp->DepAdr);
    //    break;
    //    case _I_NIBBIT:
    //    fprintf(stderr, DEPMASK,"Nibb-it",Unp->DepAdr);
    //    break;
    //    case _I_UAPACK:
    //    fprintf(stderr, DEPMASK2,"UA","Packer",Unp->DepAdr);
    //    break;
    //    case _I_ADP4KBAR:
    //    fprintf(stderr, DEPMASK,"Admiral P4Kbar/Ferris",Unp->DepAdr);
    //    break;
    //    case _I_NUCRUNCH:
    //    sprintf(appstr,"%s%s/%s","Nu","Crunch","ChristopherJam");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_TINYCRUN09:
    //    sprintf(appstr,"%s%s %s/%s","Tiny","Crunch","v0.9","ChristopherJam");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_TINYCRUNCH:
    //    sprintf(appstr,"%s%s %s/%s","Tiny","Crunch","v1.0","ChristopherJam");
    //    fprintf(stderr, DEPMASK,appstr,Unp->DepAdr);
    //    break;
    //    case _I_FLINKSDA:
    //    sprintf(appstr,"%s / %s","Linker","Agony");
    //    fprintf(stderr, DEPMASK2,"File",appstr,Unp->DepAdr);
    //    break;
    //    case _I_INCERIAPK:
    //    fprintf(stderr, DEPMASK2,"Inceria","Packer",Unp->DepAdr);
    //    break;
  }
}
