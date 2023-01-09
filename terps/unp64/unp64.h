#include "6502emu.h"
#include "exo_util.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  int IdFlag; /* flag, 1=packer identified; 2=not a packer, stop scanning */
  int Forced; /* forced entry point */
  int StrMem; /* start of unpacked memory */
  int RetAdr; /* return address after unpacking */
  int DepAdr; /* unpacker entry point */
  int EndAdr; /* end of unpacked memory */
  int RtAFrc; /* flag, return address must be exactly RetAdr, else anything >=
                 RetAdr */
  int WrMemF; /* flag, clean unwritten memory */
  int LfMemF; /* flag, clean end memory leftovers */
  int ExoFnd; /* flag, Exomizer detected */
              //             int IseFlg; /* flag, Isepic detected */
  int FStack; /* flag, fill stack with 0 and SP=$ff, else as in C64 */
  int ECAFlg; /* ECA found, holds relocated areas high bytes */
  int fEndBf; /* End memory address pointer before unpacking, set when DepAdr is
                 reached */
  int fEndAf; /* End memory address pointer after  unpacking, set when RetAdr is
                 reached */
  int fStrBf; /* Start memory address pointer before unpacking, set when DepAdr
                 is reached */
  int fStrAf; /* Start memory address pointer after  unpacking, set when RetAdr
                 is reached */
  int IdOnly; /* flag, just identify packer and exit */
  int DebugP; /* flag, verbosely emit various infos */
  int RTIFrc; /* flag, RTI instruction forces return from unpacker */
  int Recurs; /* recursion counter */
  unsigned int MonEnd; /* End memory address pointers monitored during
                          execution, updated every time DepAdr is reached */
  unsigned int MonStr; /* Start memory address pointers monitored during
                          execution, updated every time DepAdr is reached */
  unsigned int
      Mon1st; /* flag for forcingly assign monitored str/end ptr the 1st time */
  unsigned int
      EndAdC; /* add fixed values and/or registers AXY to End memory address */
  unsigned int StrAdC; /* add fixed values and/or registers AXY to Start memory
                          address */
  unsigned int Filler; /* Memory filler byte*/
  unsigned char *mem;  /* pointer to the memory array */
  char *name;          /* name of the prg file */
  struct load_info *info; /* pointer to the loaded prg info struct */
  struct cpu_ctx *r;      /* pointer to the registers struct */
} unpstr;

typedef void (*Scnptr)(unpstr *);

#define VERSION "2.35"

#define EA_USE_A 0x01000000
#define EA_USE_X 0x00100000
#define EA_USE_Y 0x00010000
#define EA_ADDFF 0x10000000
/* worst case found so far: depacking an ALZ64 program $0801-$ffff
Iterations 36260732 (0x02294B7C)
*/
#define ITERMAX 0x02000000
#define RECUMAX 16

#define DEPMASK "%s, unpacker=$%04x\n"
#define DEPMASK2 "%s %s, unpacker=$%04x\n"
#define STRMEMAJ "\nStart mem adjusted to $%04x %s\n"

extern char appstr[80];
// extern unsigned char roms[2][0x2000];

int desledge(unsigned int p, unsigned int prle, unsigned int startm,
             unsigned char *mem);
int xxOpenFile(FILE **hh, unpstr *Unp, int p);
char *normalizechar(char *app, unsigned char *m, int *ps);
void printmsg(unsigned char *mem, int start, int num);
void PrintInfo(unpstr *Unp, int id);
void Scanners(unpstr *);
void Scn_NotPackers(unpstr *);
void Scn_Intros(unpstr *);
void Scn_XIP(unpstr *);
// void Scn_OUG                (unpstr*);
// void Scn_IsePic             (unpstr*);
void Scn_Optimus(unpstr *);
void Scn_xCodeZippers(unpstr *);
void Scn_Expert(unpstr *);
void Scn_AR(unpstr *);
void Scn_BitImploder(unpstr *);
void Scn_Jox(unpstr *);
void Scn_ExplCrunch(unpstr *);
void Scn_ExplFaces(unpstr *);
void Scn_Facepacker(unpstr *);
// void Scn_ABCruncher         (unpstr*);
void Scn_BYG(unpstr *);
void Scn_FilecompactorTMC(unpstr *);
// void Scn_ASCprot            (unpstr*);
void Scn_CFB(unpstr *);
// void Scn_Zipper             (unpstr*);
void Scn_ECA(unpstr *);
void Scn_NSU(unpstr *);
void Scn_IDT(unpstr *);
void Scn_Cruel(unpstr *);
void Scn_PuCrunch(unpstr *);
void Scn_MDG(unpstr *);
void Scn_AbuzeCrunch        (unpstr*);
void Scn_SledgeHammer(unpstr *);
void Scn_TimCrunch(unpstr *);
void Scn_TimeCruncher(unpstr *);
void Scn_BetaDynamic(unpstr *);
void Scn_DarkSqueezer(unpstr *);
void Scn_ByteBoiler(unpstr *);
void Scn_MrCross(unpstr *);
// void Scn_1001card           (unpstr*);
void Scn_WDRsoftp(unpstr *);
void Scn_DSCcoder(unpstr *);
void Scn_BonanzaBros(unpstr *);
// void Scn_Agony              (unpstr*);
void Scn_ExplXRated(unpstr *);
void Scn_MasterCompressor(unpstr *);
void Scn_ScreenCrunch(unpstr *);
void Scn_MCCrackenComp(unpstr *);
void Scn_FinalSuperComp(unpstr *);
// void Scn_4cPack             (unpstr*);
void Scn_FXbytepress(unpstr *);
void Scn_FXbitstream(unpstr *);
void Scn_Trianon(unpstr *);
void Scn_KompressMaster711(unpstr *);
void Scn_TurboPacker(unpstr *);
void Scn_TCScrunch(unpstr *);
void Scn_TBCMultiComp(unpstr *);
void Scn_ByteBoozer(unpstr *);
void Scn_ALZ64(unpstr *);
void Scn_XTC(unpstr *);
void Scn_UniPacker(unpstr *);
void Scn_TCD(unpstr *);
void Scn_Matcham(unpstr *);
void Scn_Supercrunch(unpstr *);
void Scn_Superpack(unpstr *);
void Scn_BeefTrucker(unpstr *);
void Scn_TSB(unpstr *);
void Scn_ISC(unpstr *);
void Scn_TMM(unpstr *);
void Scn_FilecompactorMTB(unpstr *);
//void Scn_EqByteComp(unpstr *);
void Scn_Galleon(unpstr *);
void Scn_BeastLink(unpstr *);
void Scn_BronxPacker(unpstr *);
void Scn_Oneway(unpstr *);
void Scn_CeleriPack(unpstr *);
void Scn_CadgersPacker(unpstr *);
void Scn_Crush(unpstr *);
void Scn_Lightmizer(unpstr *);
void Scn_Frog(unpstr *);
void Scn_FrontPacker(unpstr *);
void Scn_MartinPiper(unpstr *);
// void Scn_Apack              (unpstr*);
void Scn_ONS(unpstr *);
void Scn_FC3pack(unpstr *);
void Scn_Amnesia(unpstr *);
void Scn_Polonus(unpstr *);
void Scn_KressCrunch(unpstr *);
void Scn_SirMiniPack(unpstr *);
void Scn_MRD(unpstr *);
void Scn_CCS(unpstr *);
void Scn_PZW(unpstr *);
void Scn_C4MRP(unpstr *);
void Scn_ILSCoder(unpstr *);
void Scn_MSI(unpstr *);
void Scn_U_111pack(unpstr *);
void Scn_Hawk(unpstr *);
void Scn_TSMcoder(unpstr *);
void Scn_Ikari(unpstr *);
void Scn_WGIcoder(unpstr *);
void Scn_HTLcoder(unpstr *);
void Scn_XDScoder(unpstr *);
void Scn_FDTcoder(unpstr *);
void Scn_SKLcoder(unpstr *);
void Scn_SpeediComp(unpstr *);
void Scn_U_DSC_MA(unpstr *);
void Scn_P100(unpstr *);
void Scn_MaschSprache(unpstr *);
void Scn_MegaCruncher(unpstr *);
void Scn_VIP(unpstr *);
void Scn_Gandalf(unpstr *);
void Scn_AEKcoder(unpstr *);
void Scn_LSTcoder(unpstr *);
void Scn_Eastlinker(unpstr *);
void Scn_Jazzcat(unpstr *);
void Scn_PITcoder(unpstr *);
void Scn_BitPacker(unpstr *);
void Scn_ByteCompactor(unpstr *);
void Scn_Rows(unpstr *);
void Scn_U_400All(unpstr *);
void Scn_U_439pack(unpstr *);
// void Scn_64er               (unpstr*);
void Scn_TMCcoder(unpstr *);
void Scn_ALScoder(unpstr *);
void Scn_UltraComp(unpstr *);
void Scn_FCG(unpstr *);
void Scn_Megabyte(unpstr *);
// void Scn_Zigag              (unpstr*);
void Scn_Brains(unpstr *);
void Scn_Graffity(unpstr *);
void Scn_Section8(unpstr *);
void Scn_MrZ(unpstr *);
void Scn_DD(unpstr *);
void Scn_PackOpt(unpstr *);
void Scn_Warlock(unpstr *);
void Scn_U_100pack(unpstr *);
void Scn_U_101pack(unpstr *);
void Scn_SyncroPack(unpstr *);
void Scn_Relax(unpstr *);
void Scn_Jazoo(unpstr *);
void Scn_WCC(unpstr *);
void Scn_LTS(unpstr *);
void Scn_Low(unpstr *);
void Scn_C_Noack(unpstr *);
void Scn_Koncina(unpstr *);
void Scn_U_Triad(unpstr *);
void Scn_RapEq(unpstr *);
void Scn_ByteKiller(unpstr *);
void Scn_Loadstar(unpstr *);
void Scn_Trashcan(unpstr *);
void Scn_Caution(unpstr *);
void Scn_U_25_pack(unpstr *);
void Scn_U_8e_pack(unpstr *);
void Scn_U_P3_pack(unpstr *);
void Scn_FalcoPack(unpstr *);
//void Scn_FP(unpstr *);
//void Scn_FinalCompactor(unpstr *);
void Scn_NEC(unpstr *);
void Scn_EnigmaMFFL(unpstr *);
void Scn_Shurigen(unpstr *);
void Scn_U_Generic801(unpstr *);
void Scn_Exomizer(unpstr *);
void Scn_STL(unpstr *);
void Scn_SPC(unpstr *);
void Scn_FSW(unpstr *);
void Scn_BAM(unpstr *);
void Scn_Cobra(unpstr *);
// void Scn_GrafBinaer         (unpstr*);
// void Scn_AbyssCoder         (unpstr*);
void Scn_ByteBuster(unpstr *);
void Scn_Mekker(unpstr *);
void Scn_ByteStrainer(unpstr *);
// void Scn_Jedi               (unpstr*);
void Scn_Pride(unpstr *);
void Scn_Autostarters(unpstr *);
void Scn_StarCrunch(unpstr *);
void Scn_SpyPack(unpstr *);
void Scn_GPacker(unpstr *);
void Scn_Cadaver(unpstr *);
void Scn_Helmet(unpstr *);
// void Scn_Anticom            (unpstr*);
// void Scn_Antiram            (unpstr*);
void Scn_Excalibur(unpstr *);
void Scn_CNCD(unpstr *);
void Scn_BN1872(unpstr *);
// void Scn_4wd                (unpstr*);
void Scn_Huffer(unpstr *);
void Scn_NM156(unpstr *);
void Scn_Ratt(unpstr *);
void Scn_Byterapers(unpstr *);
void Scn_CIACrypt(unpstr *);
void Scn_PAN(unpstr *);
void Scn_TKC(unpstr *);
// void Scn_THS                (unpstr*);
void Scn_UProt(unpstr *);
void Scn_YetiCoder(unpstr *);
void Scn_Panoramic(unpstr *);
void Scn_WHO(unpstr *);
void Scn_KGBcoder(unpstr *);
// void Scn_Pyra               (unpstr*);
void Scn_ActionPacker(unpstr *);
void Scn_CCrypt(unpstr *);
void Scn_TDT(unpstr *);
void Scn_Cult(unpstr *);
void Scn_Gimzo(unpstr *);
void Scn_BlackAdder(unpstr *);
void Scn_ICS8(unpstr *);
void Scn_Paradroid(unpstr *);
void Scn_Plasma(unpstr *);
void Scn_DrZoom(unpstr *);
void Scn_ExactCoder(unpstr *);
void Scn_CNETFixer(unpstr *);
// void Scn_Triangle           (unpstr*);
void Scn_SFLinker(unpstr *);
void Scn_Bongo(unpstr *);
void Scn_Jemasoft(unpstr *);
void Scn_TATCoder(unpstr *);
void Scn_RFOCoder(unpstr *);
void Scn_Doynamite(unpstr *);
void Scn_Nibbit(unpstr *);
void Scn_TLRLinker(unpstr *);
void Scn_TLRSubsizer(unpstr *);
// void Scn_AdmiralP4kbar      (unpstr*);
// void Scn_ZeroCoder          (unpstr*);
void Scn_NuCrunch(unpstr *);
void Scn_TinyCrunch(unpstr *);
void Scn_FileLinkerSDA(unpstr *);
void Scn_Inceria(unpstr *);

enum stringpool_id {
  _I_1001_4,
  _I_1001_NEWPACK,
  _I_1001_OLDPACK,
  _I_1001_CRAM,
  _I_1001_ACM,
  _I_1001_HTL,
  _I_DATELUC,
  _I_DATELUCP,
  _I_DATELUCC
  //                   , _I_4CPACK
  ,
  _I_64ER_SP22,
  _I_64ER_SP23,
  _I_64ER_SP2U,
  _I_64ER_SP12,
  _I_64ER_SP13,
  _I_64ER_SP14,
  _I_64ER_SP15,
  _I_64ER_SP1U,
  _I_64ERBITP,
  _I_64ERBITP1,
  _I_64ERS41A,
  _I_64ERS41B,
  _I_64ERS41C,
  _I_64ERS40A,
  _I_64ERS40B,
  _I_64ERS40C,
  _I_64ERS40D,
  _I_64ERS40DB,
  _I_64ER_15,
  _I_64ERS40F,
  _I_64ER_21,
  _I_HAPPYS32,
  _I_HAPPYS22,
  _I_64ERAUTO,
  _I_AB_CRUNCH
, _I_ABUZECRUNCH
, _I_ABUZECR37
, _I_ABUZECR50,
  _I_AEKCOD20,
  _I_AEKCOD11,
  _I_SYNCRO13,
  _I_SYNCRO14,
  _I_SYNCRO155,
  _I_SYNCRO12,
  _I_TLRPACK,
  _I_TLRPROT1,
  _I_TLRPROT2,
  _I_TLRLINK,
  _I_TLRLINKFM,
  _I_TLRSSIZER,
  _I_TLRSSIZD5,
  _I_TLRSSIZD6
  //                   , _I_AGONYPACK
  ,
  _I_DARKP20,
  _I_DARKP21,
  _I_DARKP31,
  _I_JAP2,
  _I_ALSCODER,
  _I_ALZ64
  //                   , _I_AMNESIA1
  //                   , _I_AMNESIA2
  //                   , _I_APACK
  //                   , _I_AR4
  //                   , _I_AR3
  //                   , _I_ARU
  //                   , _I_AR42F
  ,
  _I_SSNAP,
  _I_FRZMACH,
  _I_FRZMACH2F
  //                   , _I_ASCPROT
  ,
  _I_BEASTLINK,
  _I_BEEFTR54,
  _I_BEEFTR56,
  _I_BEEFTR2,
  _I_BEEFTLB,
  _I_BETADYN3,
  _I_BETADYN3FX,
  _I_BETADYN3CCS,
  _I_BETADYN2,
  _I_BITIMP,
  _I_BITPACK2,
  _I_BONANZA,
  _I_BRAINS,
  _I_BRONX,
  _I_BYG1,
  _I_BYG2,
  _I_BYGBB,
  _I_BYTEBOILER,
  _I_BYTEBOICPX,
  _I_BYTEBOISCS,
  _I_BYTEBOOZER,
  _I_BYTEBOOZ20,
  _I_BYTEBOOZ11C,
  _I_BYTEBOOZ11E,
  _I_LAMEBOOZER,
  _I_BYTECOMP,
  _I_BYTEKILL,
  _I_C4MRP,
  _I_CADGERS,
  _I_CAUTION10,
  _I_CAUTION25,
  _I_CAUTION25SS,
  _I_CAUTION20,
  _I_CAUTION20SS,
  _I_CAUTIONHP,
  _I_CCSMAXS,
  _I_CCSPACK,
  _I_CCSCRUNCH1,
  _I_CCSCRUNCH2,
  _I_CCSSCRE,
  _I_CCSSPEC,
  _I_CCSEXEC,
  _I_CCSUNKN,
  _I_CCSHACK,
  _I_CELERIP,
  _I_CFBCOD1,
  _I_CFBCOD2,
  _I_CRUEL22,
  _I_CRUELFAST2,
  _I_CRUEL2MHZ,
  _I_CRUEL_X,
  _I_CRUELMS15,
  _I_CRUELMS10,
  _I_CRUELMS1X,
  _I_CRUELMS_U,
  _I_CRUMSABS,
  _I_CRUELFAST4,
  _I_CRUEL_HDR,
  _I_CRUEL_H22,
  _I_CRUEL_H20,
  _I_CRUEL_HTC,
  _I_CRUEL20,
  _I_CRUEL21,
  _I_CRUEL_ILS,
  _I_CRUEL_RND,
  _I_CRUEL_TKC,
  _I_CRUEL12,
  _I_CRUEL10,
  _I_CRUEL14,
  _I_TABOOCRUSH,
  _I_TABOOCRNCH,
  _I_TABOOPACK,
  _I_TABOOPACK64,
  _I_CNOACK,
  _I_CNOACKLXT,
  _I_DARKSQ08,
  _I_DARKSQ21,
  _I_DARKSQ2X,
  _I_DARKSQ4,
  _I_DARKSQF4,
  _I_DARKSQXTC,
  _I_DARKSQWOW,
  _I_DARKSQGP,
  _I_DARKSQDOM,
  _I_DARKSQPAR,
  _I_DARKSQ4SHK,
  _I_DARKSQ22,
  _I_DARKSQBB1,
  _I_DARKSQBB2,
  _I_DDINTROC,
  _I_DSCCOD,
  _I_EASTLINK,
  _I_ECA,
  _I_ECAOLD,
  _I_ENIGMAMFFL,
  _I_EQBYTEC12,
  _I_EQBYTEC14,
  _I_EQBYTEC17,
  _I_EQBYTEC19,
  _I_EQBYTEC19TAL,
  _I_EXOM,
  _I_EXOM3,
  _I_EXOM302,
  _I_EXPERT27,
  _I_EXPERT29,
  _I_EXPERT29EUC,
  _I_EXPERT2X,
  _I_EXPERT20,
  _I_EXPERT21,
  _I_EXPERT4X,
  _I_EXPERT3X,
  _I_EXPERTASS,
  _I_EXPERT1X,
  _I_EXPERT211,
  _I_EXPLCRUNCH1X,
  _I_EXPLCRUNCH2X,
  _I_EXPLCRUNCH21,
  _I_EXPLCRUNCH22,
  _I_EXPLCOD,
  _I_EXPLFAC1,
  _I_EXPLFAC2,
  _I_XRPOWC71,
  _I_XREXPLCR,
  _I_XRPOWC74,
  _I_FACEPACK10,
  _I_FACEPACK11,
  _I_FC3PACK,
  _I_FC3LD,
  _I_FC2LD,
  _I_FCU1LD,
  _I_FCULD,
  _I_FCGPACK10,
  _I_FCGPACK30,
  _I_FCGPACK12,
  _I_FCGPACK13,
  _I_FCGLINK,
  _I_TFGPACK,
  _I_TFGCODE,
  _I_TMCULINK,
  _I_FCGCODER,
  _I_HLEISEP,
  _I_FCGPROT,
  _I_EXPROT,
  _I_FLSPROT33,
  _I_FCCPROT,
  _I_FDTCOD,
  _I_FCOMPMTB,
  _I_FCOMPTMC,
  _I_FCOMPMTC,
  _I_FCOMPK2,
  _I_FCOMPSYS3,
  _I_FINCOMP,
  _I_SUPCOMFLEX,
  _I_SUPCOMEQSE,
  _I_SUPCOMEQB9,
  _I_SUPCOMEQCCS,
  _I_SUPCOMEQC9,
  _I_SUPCOMEQCH,
  _I_SUPCOMFH11,
  _I_SUPCOMFH12,
  _I_SUPCOMFH13,
  _I_SUPCOMFH21,
  _I_SUPCOMHACK,
  _I_FPCOD,
  _I_FROGPACK,
  _I_FRONTPACK,
  _I_FXBYTEP,
  _I_FXBP_BBSP,
  _I_FXBP_BB,
  _I_FXBP_JW,
  _I_FXBP_SN,
  _I_FXBITST,
  _I_BITST11,
  _I_GALLEONEQ,
  _I_GALLCP35,
  _I_GALLCP36,
  _I_GALLCP37,
  _I_GALLCP38,
  _I_GALLCP39,
  _I_GALLCP3X,
  _I_GALLFW4C,
  _I_GALLFW4C2,
  _I_GALLFW4C3,
  _I_GALLFW4C31,
  _I_GALLBRPR,
  _I_GALLU02,
  _I_GALLU100,
  _I_GALLWHOM1,
  _I_GALLWHOM11,
  _I_GALLWHOM2,
  _I_GALLWHOM4,
  _I_GALLWHOM4S,
  _I_GALLVEST,
  _I_GANDALF,
  _I_GRAFFITY,
  _I_HAWK,
  _I_HTLCOD,
  _I_IDT10,
  _I_IDTFX21,
  _I_IDTFX20,
  _I_GNTFX20,
  _I_GNTSTATP,
  _I_EXCELCOD,
  _I_IKARICR,
  _I_ILSCOD,
  _I_IRELAX1,
  _I_IRELAX2,
  _I_IF4CG23,
  _I_ITRIAD1,
  _I_ITRIAD2,
  _I_ITRIAD5,
  _I_IGP2,
  _I_I711ID3,
  _I_IBN1872,
  _I_IEXIKARI,
  _I_IIKARI06,
  _I_IFLT01,
  _I_IS45109,
  _I_BN1872PK,
  _I_ISCN,
  _I_ISCP,
  _I_ISCBS
  //                   , _I_ISEPIC
  ,
  _I_ISEPDD,
  _I_ISEPCT1,
  _I_ISEPCT2,
  _I_ISEP7U1,
  _I_ISEP7U2,
  _I_ISEPGS1,
  _I_ISEPNC,
  _I_ISEPAMG,
  _I_ISEPGEN,
  _I_JAZOOCOD,
  _I_JCTCR,
  _I_JCTPACK,
  _I_JOXCR,
  _I_KOMPSP,
  _I_KOMP711,
  _I_KOMPBB,
  _I_KOMP71PS,
  _I_KOMP71P1,
  _I_KOMP71P2,
  _I_KONCINA,
  _I_LIGHTM,
  _I_VISIOM62,
  _I_VISIOM63,
  _I_LSMODL,
  _I_LSPACK,
  _I_LSLNK2,
  _I_LOWCR,
  _I_LSTCOD1,
  _I_LSTCOD2,
  _I_LTSPACK,
  _I_LZMPI1,
  _I_LZMPI2,
  _I_MASCHK,
  _I_MASTCOMP,
  _I_MASTCRLX,
  _I_MASTCAGL,
  _I_MASTCHF,
  _I_MATCHARP,
  _I_MATCBASP,
  _I_MATCDNEQ,
  _I_MATCDN25,
  _I_MATCDNFL,
  _I_MATCLNK2,
  _I_MATCFLEX,
  _I_MCCCOMP,
  _I_MCTSS3,
  _I_MCTSSIP,
  _I_MCUNK,
  _I_MCCRUSH3,
  _I_MCCOBRA,
  _I_MCCSCC,
  _I_MCCRYPT,
  _I_MDGPACK,
  _I_MBCR1,
  _I_MBCR2,
  _I_MEGCRBD,
  _I_MEGCR23,
  _I_MRCROSS1,
  _I_MRCROSS2,
  _I_MRDCR1,
  _I_MRDCRCOD1,
  _I_MRDLNK2,
  _I_GSSCO12,
  _I_MRZPACK,
  _I_MSICR2,
  _I_MSICR3,
  _I_MSICOD,
  _I_NECPACK,
  _I_AUSTROV1,
  _I_AUSTROV2,
  _I_AUSTROE1,
  _I_AUSTRO88,
  _I_BLITZCOM,
  _I_AUSTRO_U,
  _I_AUSTROS1,
  _I_AUSTROBL,
  _I_PETSPEED,
  _I_BASIC64,
  _I_BASICBS,
  _I_SPEEDCM,
  _I_SPEEDY64,
  _I_CC65,
  _I_CC6522,
  _I_DTL64,
  _I_LASERBAS,
  _I_HYPRA,
  _I_ISEPICLD,
  _I_NSUPACK10,
  _I_NSUPACK11,
  _I_ONSPACK,
  _I_OPTIMUS,
  _I_OPTIMH
  //                   , _I_OUGCOD
  ,
  _I_P100,
  _I_PAKOPT,
  _I_PITCOD,
  _I_POLONCB,
  _I_POLONKR,
  _I_POLONAM,
  _I_POLONEQ,
  _I_POLON4P,
  _I_POLON101,
  _I_QRTPROT,
  _I_PUCRUNCH,
  _I_PUCRUNCW,
  _I_PUCRUNCS,
  _I_PUCRUNCO,
  _I_PUCRUNCG,
  _I_PZWCOM,
  _I_RAPEQC,
  _I_RLX3B,
  _I_RLXP2,
  _I_SCRCR4,
  _I_SCRCR6,
  _I_SCRCRFCG,
  _I_S8PACK,
  _I_CRMPACK,
  _I_SHRPACK,
  _I_SIRMPACK,
  _I_SIRMLINK,
  _I_SIRCOMBIN1,
  _I_SIRCOMBIN2,
  _I_SIRCOMBIN3,
  _I_SKLCOD,
  _I_SLEDGE10,
  _I_SLEDGE11,
  _I_SLEDGE12,
  _I_SLEDGE1DP,
  _I_SLEDGE20,
  _I_SLEDGE21,
  _I_SLEDGE22,
  _I_SLEDGE23,
  _I_SLEDGE24,
  _I_SLEDGE2C,
  _I_SLEDGE30,
  _I_SLEDGETRAP,
  _I_ULTIM8SH,
  _I_MWSPCOMP,
  _I_MWPACK,
  _I_MWSPCR1,
  _I_MWSPCR2,
  _I_SUPCRUNCH,
  _I_SUPERPACK4,
  _I_TBCMULTI,
  _I_TBCMULT2,
  _I_TBCMULTMOW,
  _I_TCDLC1,
  _I_TCDLC2,
  _I_TCSCR2,
  _I_TCSCR3,
  _I_TC40,
  _I_TC42H,
  _I_TC40HIL,
  _I_TCHVIK,
  _I_TC42,
  _I_TC43,
  _I_TC44,
  _I_TC50,
  _I_TC51,
  _I_TC52,
  _I_TC53,
  _I_TC5GEN,
  _I_TC30,
  _I_TC31,
  _I_TC33,
  _I_TC2MHZ1,
  _I_TC2MHZ2,
  _I_TC32061,
  _I_TCMC4,
  _I_TC3RWE,
  _I_TC32072,
  _I_TC3RAD,
  _I_TC32074,
  _I_TC3FPE,
  _I_TC3FBI,
  _I_TC3TRI,
  _I_TC3HTL,
  _I_TC3ENT,
  _I_TCGENH,
  _I_TCFPEX,
  _I_TCSIR1,
  _I_TCUNKH,
  _I_TCSIR4,
  _I_TCSIR3,
  _I_TCSIR2,
  _I_TC5SC,
  _I_TC3RLX,
  _I_TC33AD,
  _I_TC3PC,
  _I_TC423AD,
  _I_TC42GEN,
  _I_TC3TDF,
  _I_TC3ATC,
  _I_TC3F4CG,
  _I_TC3HSCG,
  _I_TC3HSFE,
  _I_TC3TTN,
  _I_TC3MULE,
  _I_TC3AGILE,
  _I_TC3S451,
  _I_TC3S451V2,
  _I_TC3IKARI,
  _I_TC3U,
  _I_TMCCOD,
  _I_TMMPACK,
  _I_TRASHC1,
  _I_TRASHC2,
  _I_TRASHCU,
  _I_TRIANP,
  _I_TRIAN2,
  _I_TRIAN3,
  _I_TRIANC,
  _I_TSBP1,
  _I_TSBP21,
  _I_TSBP3,
  _I_TSMCOD,
  _I_TSMPACK,
  _I_TURBOP,
  _I_ULTRAC3,
  _I_UNIPACK1,
  _I_UNIPACK2,
  _I_UNIPACK3,
  _I_U_100_P2,
  _I_U_100_P3,
  _I_U_100_P4,
  _I_U_100_P5,
  _I_U_101,
  _I_U_111,
  _I_ROWS,
  _I_FALCOP,
  _I_PSQLINK1,
  _I_U_439,
  _I_ENTROPYPK,
  _I_U_8E,
  _I_U_25,
  _I_DSCCR,
  _I_DSCPK,
  _I_DSCPK2,
  _I_KRESS,
  _I_U_1WF2,
  _I_U_1WDA,
  _I_GENSYSL,
  _I_U_P3,
  _I_U_3ADCR,
  _I_VIP1X,
  _I_VIP20,
  _I_VIP3X,
  _I_TOTP1X,
  _I_TOTP20,
  _I_HIT02,
  _I_HIT20,
  _I_AGNUS02,
  _I_BUK02,
  _I_WARLOCK,
  _I_WCCMC,
  _I_WDRPROT,
  _I_WDRLAMEK,
  _I_WGICOD,
  _I_3CZIP,
  _I_4CZIP,
  _I_4CZIP2S,
  _I_XTERM,
  _I_ILSCOMP2X,
  _I_ILSCOMP3X,
  _I_3CZIP2,
  _I_XDSV1,
  _I_XDSV2,
  _I_XDSV3,
  _I_XDSK1,
  _I_XDSK2,
  _I_XDSK3,
  _I_XIP,
  _I_XTC10,
  _I_XTC21,
  _I_XTC22,
  _I_XTC23,
  _I_XTC23GP,
  _I_XTC2XGP,
  _I_ZIGAG,
  _I_ZIP50C,
  _I_ZIP50H,
  _I_ZIP511,
  _I_ZIP512,
  _I_ZIP513,
  _I_ZIP51XEN,
  _I_ZIP3,
  _I_ZIP8,
  _I_STLPROT
  //                   , _I_GRFBIN
  ,
  _I_ABYSSCOD1,
  _I_ABYSSCOD2,
  _I_SPCCOD,
  _I_SPCPROT,
  _I_COBRACOD,
  _I_FSWPACK1,
  _I_FSWPACK2,
  _I_FSWPACK3,
  _I_FSWPROT,
  _I_BAMCOD1,
  _I_BAMCOD2,
  _I_BAMCOD3,
  _I_BYTEBUST4L,
  _I_BYTEBUST4H,
  _I_MEKKER,
  _I_BYTESTRN
  //                   , _I_JEDILINK1
  //                   , _I_JEDILINK2
  //                   , _I_JEDIPACK
  ,
  _I_PRIDE,
  _I_STARCRUNCH,
  _I_SPYPACK,
  _I_PETPACK,
  _I_GPACK,
  _I_CADAVERPACK,
  _I_HELMET2
  //                   , _I_ANTICOM
  //                   , _I_ANTIRAM1
  //                   , _I_ANTIRAM2
  ,
  _I_GCSAUTO,
  _I_AUTOFLG,
  _I_EXCPACK,
  _I_EXCCOD,
  _I_CNCD,
  _I_4WD,
  _I_1WMKL2,
  _I_EMUFUX1,
  _I_EMUFUX2,
  _I_HUFFER,
  _I_HUFFERQC,
  _I_NM156PACK,
  _I_NM156LINK,
  _I_RATTPACK,
  _I_RATTLINK,
  _I_BYTERAPERS1,
  _I_BYTERAPERS2,
  _I_CIACRP,
  _I_PANPACK,
  _I_PANPACK2,
  _I_TKCEQB1,
  _I_TKCEQB2,
  _I_UPROT420,
  _I_HIVIRUS,
  _I_BHPVIRUS,
  _I_BULAVIRUS,
  _I_CODERVIRUS,
  _I_BOAVIRUS,
  _I_PLUSHZ,
  _I_PLUSHZS,
  _I_REBELP,
  _I_YETICOD,
  _I_CHGPROT2,
  _I_PDPACK1,
  _I_PDPACK2,
  _I_PDPACK30,
  _I_PDPACK31,
  _I_PDPACK32,
  _I_PDEQLZ15
  //                   , _I_THSCOD
  ,
  _I_PWCOPSH,
  _I_PWTIMEC,
  _I_PWSNACKY,
  _I_PWTCD,
  _I_PWJCH,
  _I_PWFILEPR,
  _I_PWFLT,
  _I_WHO
  //                   , _I_PYRACOD1
  //                   , _I_PYRACOD2
  ,
  _I_BFPACK,
  _I_THUNCATSPK,
  _I_ACTIONP,
  _I_KGBCOD,
  _I_CCRYPT,
  _I_TDT,
  _I_TDTPETC1,
  _I_TDTPETC2,
  _I_TDTPETCU,
  _I_TDTPETL,
  _I_CULT,
  _I_GIMZO,
  _I_BETASKIP,
  _I_BLACKADDER,
  _I_ICS8,
  _I_PARADROID1,
  _I_PARADROID2,
  _I_PARADROIDL,
  _I_PLASMACOD,
  _I_DRZOOM,
  _I_TIMCR,
  _I_EXACTCOD,
  _I_CNETFIX
  //                   , _I_TRIANGLECOD
  ,
  _I_SFLINK,
  _I_BONGO,
  _I_JEMABASIC,
  _I_TATCOD,
  _I_RFOCOD,
  _I_DOYNAMITE,
  _I_BITNAX,
  _I_NIBBIT,
  _I_UAPACK,
  _I_KREATOR,
  _I_ADP4KBAR,
  _I_ZIPLINK21,
  _I_ZIPLINK25,
  _I_ZEROCOD,
  _I_NUCRUNCH,
  _I_TINYCRUN09,
  _I_TINYCRUNCH,
  _I_FLINKSDA,
  _I_DSCTITMK,
  _I_INCERIAPK
};
