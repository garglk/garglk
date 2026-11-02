/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Module notes:
 *
 * o Adds new "types" to jAsea's property descriptor: 'M' for multiline
 *   strings, 'Z', 'F'/'T', and 'E' for defaulted integers, booleans, and
 *   strings, 'i', 'b', and 's' for ignored integers, booleans, and strings,
 *   and '{...}' and '|...|' for "special" descriptions and version fixups
 *   that can't be described as things stand.
 *
 * o Adds new 'G' expression test, to check Global boolean.
 *
 * o The stack "adjustment" stuff is a bit of a bother.
 *
 * o The '|...|' version fixup handlers live in sctafpar_fixups.cpp, and the
 *   targeted game patches in sctafpar_patches.cpp; sctafpar_internal.h
 *   declares what the three files share.
 */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "scarier.h"
#include "sctafpar_internal.h"


/* Assorted definitions and constants. */
enum
{ PARSE_TEMP_LENGTH = 256,
  PARSE_MAX_DEPTH = 32
};

/* Multiline separators; every version before 4.0 shares the same one. */
enum { SEPARATOR_SIZE = 3 };
static const scr_byte V400_SEPARATOR[SEPARATOR_SIZE] = {0xbd, 0xd0, 0x00};
static const scr_byte PRE_400_SEPARATOR[SEPARATOR_SIZE] = {0x2a, 0x2a, 0x00};


/*
 * Tables of properties descriptors.  These strings define the structure of
 * a TAF file.  Field keys are:
 *
 * $,#,B,M   - string, integer, boolean, and multiline properties
 * E,F,T,Z   - string, integer, and boolean properties not in the TAF;
 *             set to "", FALSE, TRUE and zero on parsing (version < 4)
 * i,s,b     - string, integer, and boolean in the TAF, but not stored
 * [num]     - arrays of property, fixed size to num
 * V         - variable sized array of property, size in input file
 * W         - like V, but size - 1 in input file (version < 4)
 * <class>   - class of property, separate parse target (recurse)
 * ?[!]expr: - conditional property based on expr
 * G[!]expr: - conditional property based on expr using globals
 * |...|     - fixup specials for versions < 4
 * {special} - because some things just defy description
 */
typedef struct
{
  const scr_char *const class_name;
  const scr_char *const descriptor;
} scr_parse_schema_t;

/*
 * Records whose descriptor is byte-for-byte the same in every version that
 * has them, written once here and named from the version tables below.
 * HEADER is shared by 4.0, 3.9 and 3.8 (3.7 appends a #WinTask), EVENT by
 * 4.0 and 3.9 (3.8 and 3.7 have no resources), the rest by every version
 * whose table lists them.
 */
static const scr_char V400_V390_V380_HEADER_DESC[] =
  "MStartupText #StartRoom MWinText";
static const scr_char V400_V390_EVENT_DESC[] =
  "$Short #StarterType ?#StarterType=2:#StartTime,#EndTime"
  " ?#StarterType=3:#TaskNum #RestartType BTaskFinished #Time1 #Time2"
  " $StartText $LookText $FinishText <ROOM_LIST0>Where #PauseTask"
  " BPauserCompleted #PrefTime1 $PrefText1 #ResumeTask BResumerCompleted"
  " #PrefTime2 $PrefText2 #Obj2 #Obj2Dest #Obj3 #Obj3Dest #Obj1 #Obj1Dest"
  " #TaskAffected [5]<RESOURCE>Res";
static const scr_char ROOM_LIST0_DESC[] = "#Type {ROOM_LIST0}";
static const scr_char ROOM_LIST1_DESC[] = "#Type {ROOM_LIST1}";
static const scr_char TOPIC_DESC[] = "$Subject $Reply #Task $AltReply";
static const scr_char ROOM_GROUP_DESC[] = "$Name {ROOM_GROUP:[]BList}";
static const scr_char SYNONYM_DESC[] = "$Replacement $Original";
static const scr_char ALR_DESC[] = "$Original $Replacement";

/*
 * The version 4.0 room exit record descriptor; parse_special() matches on
 * this same constant.
 */
static const scr_char V400_ROOM_EXIT_DESC[] =
  "{V400_ROOM_EXIT:#Dest_#Var1_#Var2_#Var3}";

/* Version 4.0 TAF file properties descriptor table. */
static const scr_parse_schema_t V400_PARSE_SCHEMA[] = {
  {"_GAME_",
   "<HEADER>Header <GLOBAL>Globals V<ROOM>Rooms V<OBJECT>Objects V<TASK>Tasks"
   " V<EVENT>Events V<NPC>NPCs V<ROOM_GROUP>RoomGroups V<SYNONYM>Synonyms"
   " V<VARIABLE>Variables V<ALR>ALRs BCustomFont ?BCustomFont:$FontNameSize"
   " $CompileDate"},
  {"HEADER", V400_V390_V380_HEADER_DESC},
  {"GLOBAL",
   "$GameName $GameAuthor $DontUnderstand #Perspective BShowExits #WaitTurns"
   " BDispFirstRoom BBattleSystem #MaxScore $PlayerName BPromptName $PlayerDesc"
   " #Task ?!#Task=0:$AltDesc #Position #ParentObject #PlayerGender"
   " #MaxSize #MaxWt ?GBattleSystem:<BATTLE>Battle BEightPointCompass bNoDebug"
   " BNoScoreNotify BNoMap bNoAutoComplete bNoControlPanel bNoMouse BSound"
   " BGraphics <RESOURCE>IntroRes <RESOURCE>WinRes BStatusBox $StatusBoxText"
   " #SizeMultiple #WeightMultiple BEmbedded"},
  {"BATTLE",
   "#StaminaLo #StaminaHi #StrengthLo #StrengthHi #AccuracyLo #AccuracyHi"
   " #DefenseLo #DefenseHi #AgilityLo #AgilityHi #Recovery"},
  {"ROOM",
   "$Short $Long ?GEightPointCompass:[12]<ROOM_EXIT>Exits"
   " ?!GEightPointCompass:[8]<ROOM_EXIT>Exits <RESOURCE>Res V<ROOM_ALT>Alts"
   " ?!GNoMap:BHideOnMap"},
  {"ROOM_EXIT", V400_ROOM_EXIT_DESC},
  {"ROOM_ALT",
   "$M1 #Type <RESOURCE>Res1 $M2 #Var2 <RESOURCE>Res2 #HideObjects $Changed"
   " #Var3 #DisplayRoom"},
  {"RESOURCE",
   "?GSound:$SoundFile,#SoundLen,ZSoundOffset"
   " ?GGraphics:$GraphicFile,#GraphicLen,ZGraphicOffset {V400_RESOURCE}"},
  {"OBJECT",
   "$Prefix $Short V$Alias BStatic $Description #InitialPosition #Task"
   " BTaskNotDone $AltDesc ?BStatic:<ROOM_LIST1>Where BContainer BSurface"
   " #Capacity ?!BStatic:BWearable,#SizeWeight,#Parent"
   " ?BStatic:{OBJECT:#Parent} #Openable ?#Openable=5:#Key ?#Openable=6:#Key"
   " ?#Openable=7:#Key #SitLie ?!BStatic:BEdible BReadable ?BReadable:$ReadText"
   " ?!BStatic:BWeapon #CurrentState ?!#CurrentState=0:$States,BStateListed"
   " BListFlag <RESOURCE>Res1 <RESOURCE>Res2 ?GBattleSystem:<OBJ_BATTLE>Battle"
   " $InRoomDesc #OnlyWhenNotMoved"},
  {"OBJ_BATTLE",
   "#ProtectionValue #HitValue #Method #Accuracy"},
  {"ROOM_LIST1", ROOM_LIST1_DESC},
  {"TASK",
   "V$Command $CompleteText $ReverseMessage $RepeatText $AdditionalMessage"
   " #ShowRoomDesc BRepeatable BReversible V$ReverseCommand <ROOM_LIST0>Where"
   " $Question ?$Question:$Hint1,$Hint2 V<TASK_RESTR>Restrictions"
   " V<TASK_ACTION>Actions $RestrMask <RESOURCE>Res"},
  {"TASK_RESTR",
   "#Type ?#Type=0:#Var1,#Var2,#Var3 ?#Type=1:#Var1,#Var2 ?#Type=2:#Var1,#Var2"
   " ?#Type=3:#Var1,#Var2,#Var3 ?#Type=4:#Var1,#Var2,#Var3,$Var4"
   " |V400_TASK_RESTR:Type>4?#Var1,#Var2,#Var3| $FailMessage"},
  {"TASK_ACTION",
   "#Type ?#Type=0:#Var1,#Var2,#Var3 ?#Type=1:#Var1,#Var2,#Var3"
   " ?#Type=2:#Var1,#Var2 ?#Type=3:#Var1,#Var2,#Var3,$Expr,#Var5"
   " ?#Type=4:#Var1 ?#Type=5:#Var1,#Var2 ?#Type=6:#Var1,#Var2,#Var3"
   " ?#Type=7:#Var1,#Var2,#Var3"},
  {"ROOM_LIST0", ROOM_LIST0_DESC},
  {"EVENT", V400_V390_EVENT_DESC},
  {"NPC",
   "$Name $Prefix V$Alias $Descr #StartRoom $AltText #Task V<TOPIC>Topics"
   " V<WALK>Walks BShowEnterExit ?BShowEnterExit:$EnterText,$ExitText"
   " $InRoomText #Gender [4]<RESOURCE>Res ?GBattleSystem:<NPC_BATTLE>Battle"},
  {"NPC_BATTLE",
   "#Attitude #StaminaLo #StaminaHi #StrengthLo #StrengthHi #AccuracyLo"
   " #AccuracyHi #DefenseLo #DefenseHi #AgilityLo #AgilityHi #Speed"
   " #KilledTask #Recovery #StaminaTask"},
  {"TOPIC", TOPIC_DESC},
  {"WALK",
   "#NumStops BLoop #StartTask #CharTask #MeetObject #ObjectTask #StoppingTask"
   " #MeetChar $ChangedDesc {WALK:#Rooms_#Times}"},
  {"ROOM_GROUP", ROOM_GROUP_DESC},
  {"SYNONYM", SYNONYM_DESC},
  {"VARIABLE",
   "$Name #Type $Value"},
  {"ALR", ALR_DESC},
  {nullptr, nullptr}
};

/*
 * The room exit record is byte-for-byte identical in versions 3.9, 3.8 and
 * 3.7, so those three schemas share one descriptor string; parse_special()
 * matches on this same constant.
 */
static const scr_char V390_V380_ROOM_EXIT_DESC[] =
  "{V390_V380_ROOM_EXIT:#Dest_#Var1_#Var2_ZVar3}";

/* Version 3.9 TAF file properties descriptor table. */
static const scr_parse_schema_t V390_PARSE_SCHEMA[] = {
  {"_GAME_",
   "<HEADER>Header <GLOBAL>Globals V<ROOM>Rooms V<OBJECT>Objects V<TASK>Tasks"
   " V<EVENT>Events V<NPC>NPCs V<ROOM_GROUP>RoomGroups V<SYNONYM>Synonyms"
   " V<VARIABLE>Variables V<ALR>ALRs BCustomFont ?BCustomFont:$FontNameSize"
   " $CompileDate sPassword"},
  {"HEADER", V400_V390_V380_HEADER_DESC},
  {"GLOBAL",
   "$GameName $GameAuthor $DontUnderstand #Perspective BShowExits #WaitTurns"
   " BDispFirstRoom BBattleSystem #MaxScore $PlayerName BPromptName $PlayerDesc"
   " #Task ?!#Task=0:$AltDesc #Position #ParentObject #PlayerGender"
   " #MaxSize #MaxWt ?GBattleSystem:<BATTLE>Battle BEightPointCompass bNoDebug"
   " BNoScoreNotify BNoMap bNoAutoComplete bNoControlPanel bNoMouse"
   " BSound BGraphics <RESOURCE>IntroRes <RESOURCE>WinRes FStatusBox"
   " EStatusBoxText #SizeMultiple #WeightMultiple FEmbedded"},
  {"BATTLE",
   "#Stamina #Strength #Defense"},
  {"ROOM",
   "$Short $Long $LastDesc ?GEightPointCompass:[12]<ROOM_EXIT>Exits"
   " ?!GEightPointCompass:[8]<ROOM_EXIT>Exits $AddDesc1 #Task1 $AddDesc2 #Task2"
   " #Obj $AltDesc #TypeHideObjects <RESOURCE>Res <RESOURCE>LastRes"
   " <RESOURCE>Task1Res <RESOURCE>Task2Res <RESOURCE>AltRes"
   " ?!GNoMap:BHideOnMap |V390_ROOM:_Alts_|"},
  {"ROOM_EXIT", V390_V380_ROOM_EXIT_DESC},
  {"RESOURCE",
   "?GSound:$SoundFile,ZSoundLen,ZSoundOffset"
   " ?GGraphics:$GraphicFile,ZGraphicLen,ZGraphicOffset"},
  {"OBJECT",
   "$Prefix $Short"
   " [1]$Alias BStatic $Description #InitialPosition #Task BTaskNotDone"
   " $AltDesc ?BStatic:<ROOM_LIST1>Where BContainer BSurface #Capacity"
   " ?!BStatic:BWearable,#SizeWeight,#Parent ?BStatic:{OBJECT:#Parent}"
   " #Openable |V390_OBJECT:_Openable_,Key| #SitLie ?!BStatic:BEdible BReadable"
   " ?BReadable:$ReadText ?!BStatic:BWeapon ZCurrentState FListFlag"
   " <RESOURCE>Res1 <RESOURCE>Res2 ?GBattleSystem:<OBJ_BATTLE>Battle"
   " EInRoomDesc ZOnlyWhenNotMoved"},
  {"OBJ_BATTLE",
   "#ProtectionValue #HitValue #Method"},
  {"ROOM_LIST1", ROOM_LIST1_DESC},
  {"TASK",
   "W$Command $CompleteText $ReverseMessage $RepeatText $AdditionalMessage"
   " #ShowRoomDesc BRepeatable BReversible W$ReverseCommand <ROOM_LIST0>Where"
   " $Question ?$Question:$Hint1,$Hint2 V<TASK_RESTR>Restrictions"
   " V<TASK_ACTION>Actions |V390_TASK:$RestrMask| <RESOURCE>Res"},
  {"TASK_RESTR",
   "#Type ?#Type=0:#Var1,#Var2,#Var3 ?#Type=1:#Var1,#Var2 ?#Type=2:#Var1,#Var2"
   " ?#Type=3:#Var1,#Var2,#Var3 ?#Type=4:#Var1,#Var2,#Var3,EVar4"
   ",|V390_TASK_RESTR:Var1>0?#Var1++| $FailMessage"},
  {"TASK_ACTION",
   "#Type |V390_TASK_ACTION:Type>4?#Type++| ?#Type=0:#Var1,#Var2,#Var3"
   " ?#Type=1:#Var1,#Var2,#Var3 ?#Type=2:#Var1,#Var2"
   " ?#Type=3:#Var1,#Var2,#Var3,|V390_TASK_ACTION:$Expr_#Var5|"
   " ?#Type=4:#Var1 ?#Type=6:#Var1,ZVar2,ZVar3"
   " ?#Type=7:#Var1,#Var2,#Var3,|V390_TASK_ACTION:_BattleAttr_|"},
  {"ROOM_LIST0", ROOM_LIST0_DESC},
  {"EVENT", V400_V390_EVENT_DESC},
  {"NPC",
   "$Name $Prefix [1]$Alias $Descr #StartRoom $AltText #Task V<TOPIC>Topics"
   " V<WALK>Walks BShowEnterExit ?BShowEnterExit:$EnterText,$ExitText"
   " $InRoomText #Gender [4]<RESOURCE>Res ?GBattleSystem:<NPC_BATTLE>Battle"},
  {"NPC_BATTLE",
   "#Attitude #Stamina #Strength #Defense #Speed #KilledTask"},
  {"TOPIC", TOPIC_DESC},
  {"WALK",
   "#NumStops BLoop #StartTask #CharTask #MeetObject #ObjectTask #StoppingTask"
   " ZMeetChar $ChangedDesc {WALK:#Rooms_#Times}"},
  {"ROOM_GROUP", ROOM_GROUP_DESC},
  {"SYNONYM", SYNONYM_DESC},
  {"VARIABLE",
   "$Name ZType $Value"},
  {"ALR", ALR_DESC},
  {nullptr, nullptr}
};

/*
 * Descriptor strings shared verbatim by the version 3.8 and version 3.7
 * schemas, so that the version 3.7 table below holds only its genuine
 * divergences from version 3.8.
 */
static const scr_char V380_V370_GLOBAL_DESC[] =
  "$GameName $GameAuthor #MaxCarried |V380_MaxSize_MaxWt_| $DontUnderstand"
  " #Perspective BShowExits #WaitTurns FDispFirstRoom FBattleSystem"
  " EPlayerName FPromptName EPlayerDesc ZTask ZPosition ZParentObject"
  " ZPlayerGender FEightPointCompass TNoScoreNotify FSound FGraphics"
  " FStatusBox EStatusBoxText FEmbedded";
static const scr_char V380_V370_ROOM_DESC[] =
  "$Short $Long $LastDesc [8]<ROOM_EXIT>Exits $AddDesc1 #Task1 $AddDesc2"
  " #Task2 #Obj $AltDesc #TypeHideObjects |V380_ROOM:_Alts_|";
static const scr_char V380_V370_OBJECT_DESC[] =
  "$Prefix $Short [1]$Alias BStatic $Description #InitialPosition #Task"
  " BTaskNotDone $AltDesc ?BStatic:<ROOM_LIST1>Where #SurfaceContainer"
  " FSurface ?#SurfaceContainer=2:TSurface FContainer"
  " ?#SurfaceContainer=1:TContainer #Capacity |V380_OBJECT:#Capacity*10+2|"
  " ?!BStatic:BWearable,#SizeWeight,|V380_OBJECT:_SizeWeight_|,#Parent"
  " ?BStatic:{OBJECT:#Parent}"
  " #Openable |V380_OBJECT:_Openable_,Key| #SitLie ?!BStatic:BEdible BReadable"
  " ?BReadable:$ReadText ?!BStatic:BWeapon ZCurrentState FListFlag"
  " EInRoomDesc ZOnlyWhenNotMoved";
static const scr_char V380_V370_EVENT_DESC[] =
  "$Short #StarterType ?#StarterType=2:#StartTime,#EndTime"
  " ?#StarterType=3:#TaskNum #RestartType BTaskFinished #Time1 #Time2"
  " $StartText $LookText $FinishText <ROOM_LIST0>Where #PauseTask"
  " BPauserCompleted #PrefTime1 $PrefText1 #ResumeTask BResumerCompleted"
  " #PrefTime2 $PrefText2 #Obj2 #Obj2Dest #Obj3 #Obj3Dest #Obj1 #Obj1Dest"
  " #TaskAffected";
static const scr_char V380_V370_NPC_DESC[] =
  "$Name $Prefix [1]$Alias $Descr #StartRoom $AltText #Task V<TOPIC>Topics"
  " V<WALK>Walks BShowEnterExit ?BShowEnterExit:$EnterText,$ExitText"
  " $InRoomText ZGender";
static const scr_char V380_V370_WALK_DESC[] =
  "#NumStops BLoop #StartTask #CharTask #MeetObject"
  " ?!#MeetObject=0:|V380_WALK:_MeetObject_| #ObjectTask ZMeetChar"
  " {WALK:#Rooms_#Times} ZStoppingTask EChangedDesc";

/* Version 3.8 TAF file properties descriptor table. */
static const scr_parse_schema_t V380_PARSE_SCHEMA[] = {
  {"_GAME_",
   "<HEADER>Header <GLOBAL>Globals V<ROOM>Rooms V<OBJECT>Objects V<TASK>Tasks"
   " V<EVENT>Events V<NPC>NPCs V<ROOM_GROUP>RoomGroups V<SYNONYM>Synonyms"
   " FCustomFont $CompileDate sPassword |V380_GLOBAL:_MaxScore_|"
   " |V380_OBJECT:_InitialPositions_|"},
  {"HEADER", V400_V390_V380_HEADER_DESC},
  {"GLOBAL", V380_V370_GLOBAL_DESC},
  {"ROOM", V380_V370_ROOM_DESC},
  {"ROOM_EXIT", V390_V380_ROOM_EXIT_DESC},
  {"OBJECT", V380_V370_OBJECT_DESC},
  {"ROOM_LIST1", ROOM_LIST1_DESC},
  {"TASK",
   "W$Command $CompleteText $ReverseMessage $RepeatText $AdditionalMessage"
   " #ShowRoomDesc BRepeatable #Score BSingleScore [6]<TASK_MOVE>Movements"
   " BReversible W$ReverseCommand #WearObj1 #WearObj2 #HoldObj1 #HoldObj2"
   " #HoldObj3 #Obj1 #Task BTaskNotDone $TaskMsg $HoldMsg $WearMsg $CompanyMsg"
   " BNotInSameRoom #NPC $Obj1Msg #Obj1Room <ROOM_LIST0>Where BKillsPlayer"
   " BHoldingSameRoom $Question ?$Question:$Hint1,$Hint2 #Obj2"
   " ?!#Obj2=0:#Obj2Var1,#Obj2Var2,$Obj2Msg BWinGame |V380_TASK:_Actions_|"
   " |V380_TASK:_Restrictions_|"},
  {"TASK_MOVE",
   "#Var1 #Var2 #Var3"},
  {"ROOM_LIST0", ROOM_LIST0_DESC},
  {"EVENT", V380_V370_EVENT_DESC},
  {"NPC", V380_V370_NPC_DESC},
  {"TOPIC", TOPIC_DESC},
  {"WALK", V380_V370_WALK_DESC},
  {"ROOM_GROUP", ROOM_GROUP_DESC},
  {"SYNONYM", SYNONYM_DESC},
  {nullptr, nullptr}
};

/*
 * Version 3.7 TAF file properties descriptor table.
 *
 * A version 3.7 file is a version 3.8 file in all but four places, all of them
 * settled by parsing the only two version 3.7 games known to survive (arlo.taf
 * and castle.taf; see test/adrift4/notes/ADRIFT_370.md).  Both parse to
 * exactly end-of-file under this schema.
 *
 *   o The header carries one extra integer after the win text, the index of
 *     the task that wins the game (0-based).  Version 3.8 replaced it with a
 *     per-task BWinGame flag, which is why the flag is missing from the task
 *     record below -- see |V370_TASK:_WinTask_|.
 *
 *   o A task movement is a pair, not a triple: version 3.7 has a single flat
 *     destination list where 3.8 has separate "where" and "how" fields, so
 *     there is no Var3 -- see parse_fixup_v370_movement().
 *
 *   o Where version 3.8 has a general synonyms table, version 3.7 has a fixed
 *     block of seventeen strings, the (renameable) words for its built-in
 *     commands -- see |V370_GLOBAL:_Synonyms_|.
 *
 *   o Room exit, object and NPC records are byte-for-byte the version 3.8
 *     ones, so they share its fixups, and with them its pooled burden model
 *     (see obj_get_burden()).  The one semantic difference is that an object
 *     cannot start out on an NPC, so its #Parent is ignored when it starts
 *     held or worn -- see |V370_OBJECT:_InitialPositions_|.
 *
 * All four, and the burden model, were then confirmed against the genuine
 * 3.70 Runner (run370.exe under Wine); see
 * test/adrift4/notes/RUNNER_TESTS_TODO.md.
 */
static const scr_parse_schema_t V370_PARSE_SCHEMA[] = {
  {"_GAME_",
   "<HEADER>Header <GLOBAL>Globals V<ROOM>Rooms V<OBJECT>Objects V<TASK>Tasks"
   " V<EVENT>Events V<NPC>NPCs V<ROOM_GROUP>RoomGroups"
   " [17]<COMMAND>Commands FCustomFont $CompileDate sPassword"
   " |V380_GLOBAL:_MaxScore_| |V370_OBJECT:_InitialPositions_|"
   " |V370_GLOBAL:_Synonyms_| |V370_TASK:_WinTask_|"},
  {"HEADER",
   "MStartupText #StartRoom MWinText #WinTask"},
  {"GLOBAL", V380_V370_GLOBAL_DESC},
  {"ROOM", V380_V370_ROOM_DESC},
  {"ROOM_EXIT", V390_V380_ROOM_EXIT_DESC},
  {"OBJECT", V380_V370_OBJECT_DESC},
  {"ROOM_LIST1", ROOM_LIST1_DESC},
  {"TASK",
   "W$Command $CompleteText $ReverseMessage $RepeatText $AdditionalMessage"
   " #ShowRoomDesc BRepeatable #Score BSingleScore [6]<TASK_MOVE>Movements"
   " BReversible W$ReverseCommand #WearObj1 #WearObj2 #HoldObj1 #HoldObj2"
   " #HoldObj3 #Obj1 #Task BTaskNotDone $TaskMsg $HoldMsg $WearMsg $CompanyMsg"
   " BNotInSameRoom #NPC $Obj1Msg #Obj1Room <ROOM_LIST0>Where BKillsPlayer"
   " BHoldingSameRoom $Question ?$Question:$Hint1,$Hint2 #Obj2"
   " ?!#Obj2=0:#Obj2Var1,#Obj2Var2,$Obj2Msg |V370_TASK:_Actions_|"
   " |V380_TASK:_Restrictions_|"},
  {"TASK_MOVE",
   "#Var1 #Var2"},
  {"ROOM_LIST0", ROOM_LIST0_DESC},
  {"EVENT", V380_V370_EVENT_DESC},
  {"NPC", V380_V370_NPC_DESC},
  {"TOPIC", TOPIC_DESC},
  {"WALK", V380_V370_WALK_DESC},
  {"ROOM_GROUP", ROOM_GROUP_DESC},
  {"COMMAND",
   "$Word"},
  {nullptr, nullptr}
};


/*
 * Per-version parse descriptors, tying together everything that varies with
 * the TAF file version: the parse schema, the multiline separator, the fixup
 * special handler, and the printable version string.
 */
typedef struct
{
  const scr_int version;
  const scr_parse_schema_t *const schema;
  const scr_byte *const separator;
  void (*const fixup) (const scr_char *);
  const scr_char *const version_string;
} scr_parse_version_t;

static const scr_parse_version_t PARSE_VERSIONS[] = {
  {TAF_VERSION_400, V400_PARSE_SCHEMA, V400_SEPARATOR,
   parse_fixup_v400, "4.00"},
  {TAF_VERSION_390, V390_PARSE_SCHEMA, PRE_400_SEPARATOR,
   parse_fixup_v390, "3.90"},
  {TAF_VERSION_380, V380_PARSE_SCHEMA, PRE_400_SEPARATOR,
   parse_fixup_v380, "3.80"},
  {TAF_VERSION_370, V370_PARSE_SCHEMA, PRE_400_SEPARATOR,
   parse_fixup_v370, "3.70"}
};


/*
 * parse_select_version()
 *
 * Select the per-version parse descriptor based on a TAF file.
 */
static const scr_parse_version_t *
parse_select_version (scr_tafref_t taf)
{
  const scr_int version = taf_get_version (taf);

  for (const scr_parse_version_t &entry : PARSE_VERSIONS)
    {
      if (entry.version == version)
        return &entry;
    }

  scr_fatal ("parse_select_version: invalid TAF file version\n");
}


/* The uncompressed TAF file from which we get all our data. */
static scr_tafref_t parse_taf = (scr_tafref_t) nullptr;
static scr_int parse_tafline = 0;

/* The parse descriptor selected for this TAF file's version. */
static const scr_parse_version_t *parse_version = nullptr;

/* Properties bundle and trace flag, set before parsing; shared with the
   fixups and patches files through sctafpar_internal.h. */
scr_prop_setref_t parse_bundle = (scr_prop_setref_t) nullptr;
scr_bool parse_trace = FALSE;

/*
 * Stack of property keys.  The stack is filled by parsing, and written
 * to the property store on parse terminals.
 */
static scr_vartype_t parse_vt_key[PARSE_MAX_DEPTH];
static scr_char parse_format[PARSE_MAX_DEPTH];
static scr_int parse_depth = 0;


/*
 * parse_push_key()
 * parse_pop_key()
 *
 * Push a key of the given type onto the property key stack, and pop a key
 * off on unwind.
 */
void
parse_push_key (scr_vartype_t vt_key, scr_char type)
{
  if (parse_depth == PARSE_MAX_DEPTH)
    scr_fatal ("parse_push_key: stack overrun\n");

  /* Push the key, and its associated type. */
  parse_vt_key[parse_depth] = vt_key;
  parse_format[parse_depth] = type;
  parse_depth++;
}

void
parse_pop_key (void)
{
  /* Check the stack has something to pop, then pop it. */
  if (parse_depth == 0)
    scr_fatal ("parse_pop_key: stack underrun\n");
  parse_depth--;
}


/*
 * parse_retrieve_stack()
 *
 * This is ugly.  The parse produces indexes before the things that they
 * index.  An expedient fix is to switch i-s keys before storing a property
 * value
 */
static void
parse_retrieve_stack (scr_char format[], scr_vartype_t vt_key[], scr_int *depth)
{
  scr_int index_;

  /* Switch index-string key pairs. */
  for (index_ = 0; index_ < parse_depth; index_++)
    {
      if (index_ < parse_depth - 1
          && parse_format[index_] == PROP_KEY_INTEGER
          && parse_format[index_ + 1] == PROP_KEY_STRING)
        {
          /* Swap format and key elements. */
          format[index_] = parse_format[index_ + 1];
          format[index_ + 1] = parse_format[index_];
          vt_key[index_] = parse_vt_key[index_ + 1];
          vt_key[index_ + 1] = parse_vt_key[index_];

          index_++;
        }
      else
        {
          /* Simple copy of format and key elements. */
          format[index_] = parse_format[index_];
          vt_key[index_] = parse_vt_key[index_];
        }
    }

  /* Return the parse depth. */
  *depth = parse_depth;
}


/*
 * parse_stack_backtrace()
 *
 * Dump the parse stack.  Used for diagnostics on finding what we think may
 * be a bad game.
 */
static void
parse_stack_backtrace (void)
{
  scr_vartype_t vt_key[PARSE_MAX_DEPTH];
  scr_char format[PARSE_MAX_DEPTH];
  scr_int depth, index_;

  parse_retrieve_stack (format, vt_key, &depth);

  scr_error ("parse_stack_backtrace: version %s schema parsed to depth %ld\n",
            parse_version ? parse_version->version_string : "[Invalid]",
            depth);

  scr_error ("parse_stack_backtrace: parse stack backtrace follows...\n");
  for (index_ = 0; index_ < depth; index_++)
    {
      scr_char type;

      type = format[index_];
      if (type == PROP_KEY_INTEGER)
        scr_error ("%2ld - [%c] %ld\n", index_, type, vt_key[index_].integer);
      else if (type == PROP_KEY_STRING)
        scr_error ("%2ld - [%c] \"%s\"\n", index_, type, vt_key[index_].string);
      else
        scr_error ("%2ld - [%c] %p\n", index_, type, vt_key[index_].voidp);
    }
}


/*
 * parse_stack_format()
 *
 * Shared by parse_put_property() and parse_get_property(): retrieve the
 * adjusted key stack into vt_key, and complete format as the property type,
 * the two-character direction ("->" or "<-"), then one type character per
 * stacked key.  Both arrays are sized PARSE_MAX_DEPTH (+ 4 for the format).
 */
static void
parse_stack_format (scr_char format[], scr_vartype_t vt_key[],
                    scr_char type, const scr_char *direction)
{
  scr_int depth;

  /* Retrieve the adjusted stack. */
  parse_retrieve_stack (format + 3, vt_key, &depth);

  /* Complete the format for the property access. */
  format[0] = type;
  format[1] = direction[0];
  format[2] = direction[1];
  format[depth + 3] = NUL;
}


/*
 * parse_put_property()
 * parse_get_property()
 *
 * Write or read a property based on the keys amassed so far.
 */
static void
parse_put_property (scr_vartype_t vt_value, scr_char type)
{
  scr_vartype_t vt_key[PARSE_MAX_DEPTH];
  scr_char format[PARSE_MAX_DEPTH + 4];

  parse_stack_format (format, vt_key, type, "->");

  /* Store the property under the stacked keys. */
  assert (parse_bundle);
  prop_put (parse_bundle, format, vt_value, vt_key);
}

static scr_bool
parse_get_property (scr_vartype_t *vt_rvalue, scr_char type)
{
  scr_vartype_t vt_key[PARSE_MAX_DEPTH];
  scr_char format[PARSE_MAX_DEPTH + 4];

  parse_stack_format (format, vt_key, type, "<-");

  /* Retrieve the property using the stacked keys. */
  assert (parse_bundle);
  return prop_get (parse_bundle, format, vt_rvalue, vt_key);
}


/*
 * parse_get_child_count()
 *
 * Convenience form of parse_get_property(), retrieve an integer property
 * indicating the child count of the effectively stacked node, or zero if
 * no such node exists.
 */
static scr_int
parse_get_child_count (void)
{
  scr_vartype_t vt_rvalue;

  if (!parse_get_property (&vt_rvalue, PROP_INTEGER))
    vt_rvalue.integer = 0;

  return vt_rvalue.integer;
}


/*
 * parse_checked_multiply()
 *
 * Multiply an attacker-controlled child count by a fixed element size,
 * fataling on genuine integer overflow rather than silently under-allocating
 * (which could let a later fill run off the end of the block).  The counts
 * come from parse_get_child_count(), so each already consumed real TAF bytes
 * and overflow is not reachable from a valid file; this is hardening against a
 * hostile or corrupt TAF only, and deliberately imposes no arbitrary cap that
 * might reject a large-but-valid game.
 */
size_t
parse_checked_multiply (scr_int count, size_t element_size)
{
  if (count < 0)
    scr_fatal ("parse_checked_multiply: negative count %ld\n", (long) count);
  if (element_size > 0 && (size_t) count > ((size_t) -1) / element_size)
    scr_fatal ("parse_checked_multiply: size overflow (%ld * %lu bytes)\n",
               (long) count, (unsigned long) element_size);
  return (size_t) count * element_size;
}


/*
 * parse_checked_count()
 *
 * Validate an attacker-controlled child count used to size a std::vector and
 * return it as a size_t.  std::vector already does its own checked allocation
 * (it throws on overflow rather than under-allocating), but a negative parsed
 * count would convert to an enormous size_type and, worse, be used directly as
 * an index into the sized vector.  Reject it here with a clean scr_fatal
 * unwind instead of leaving an out-of-range index or an uncaught C++ throw.
 */
size_t
parse_checked_count (scr_int count)
{
  if (count < 0)
    scr_fatal ("parse_checked_count: negative count %ld\n", (long) count);
  return (size_t) count;
}


/*
 * parse_get_integer_property()
 * parse_get_boolean_property()
 * parse_get_string_property()
 *
 * Convenience forms of parse_get_property(), retrieve directly, and report
 * a fatal error if the property does not exist.
 */
static scr_int
parse_get_integer_property (void)
{
  scr_vartype_t vt_rvalue;

  if (!parse_get_property (&vt_rvalue, PROP_INTEGER))
    scr_fatal ("parse_get_integer_property: missing property\n");

  return vt_rvalue.integer;
}

static scr_bool
parse_get_boolean_property (void)
{
  scr_vartype_t vt_rvalue;

  if (!parse_get_property (&vt_rvalue, PROP_BOOLEAN))
    scr_fatal ("parse_get_boolean_property: missing property\n");

  return vt_rvalue.boolean;
}

static const scr_char *
parse_get_string_property (void)
{
  scr_vartype_t vt_rvalue;

  if (!parse_get_property (&vt_rvalue, PROP_STRING))
    scr_fatal ("parse_get_string_property: missing property\n");

  return vt_rvalue.string;
}


/*
 * parse_push_string_key()
 *
 * Push a string key, the first step of every keyed access below.
 */
static void
parse_push_string_key (const scr_char *key)
{
  scr_vartype_t vt_key;

  vt_key.string = key;
  parse_push_key (vt_key, PROP_KEY_STRING);
}


/*
 * parse_get_keyed_integer()
 * parse_get_keyed_boolean()
 * parse_get_keyed_string()
 * parse_get_keyed_child_count()
 * parse_put_keyed_integer()
 * parse_put_keyed_string()
 *
 * Convenience wrappers that push the given string key, get or put the
 * property, and pop the key again.  Almost every property access in the
 * specials and fixups below is this push-access-pop dance.
 */
scr_int
parse_get_keyed_integer (const scr_char *key)
{
  scr_int retval;

  parse_push_string_key (key);
  retval = parse_get_integer_property ();
  parse_pop_key ();

  return retval;
}

scr_bool
parse_get_keyed_boolean (const scr_char *key)
{
  scr_bool retval;

  parse_push_string_key (key);
  retval = parse_get_boolean_property ();
  parse_pop_key ();

  return retval;
}

const scr_char *
parse_get_keyed_string (const scr_char *key)
{
  const scr_char *retval;

  parse_push_string_key (key);
  retval = parse_get_string_property ();
  parse_pop_key ();

  return retval;
}

scr_int
parse_get_keyed_child_count (const scr_char *key)
{
  scr_int retval;

  parse_push_string_key (key);
  retval = parse_get_child_count ();
  parse_pop_key ();

  return retval;
}

void
parse_put_keyed_integer (const scr_char *key, scr_int value)
{
  scr_vartype_t vt_value;

  parse_push_string_key (key);
  vt_value.integer = value;
  parse_put_property (vt_value, PROP_INTEGER);
  parse_pop_key ();
}

void
parse_put_keyed_string (const scr_char *key, const scr_char *value)
{
  scr_vartype_t vt_value;

  parse_push_string_key (key);
  vt_value.string = value;
  parse_put_property (vt_value, PROP_STRING);
  parse_pop_key ();
}


/*
 * parse_put_indexed_property()
 * parse_put_indexed_integer()
 * parse_put_indexed_boolean()
 *
 * As above, but store list_key[index_] = value, pushing both the list key
 * string and the element index.
 */
static void
parse_put_indexed_property (const scr_char *list_key, scr_int index_,
                            scr_vartype_t vt_value, scr_char type)
{
  scr_vartype_t vt_key;

  parse_push_string_key (list_key);
  vt_key.integer = index_;
  parse_push_key (vt_key, PROP_KEY_INTEGER);
  parse_put_property (vt_value, type);
  parse_pop_key ();
  parse_pop_key ();
}

static void
parse_put_indexed_integer (const scr_char *list_key,
                           scr_int index_, scr_int value)
{
  scr_vartype_t vt_value;

  vt_value.integer = value;
  parse_put_indexed_property (list_key, index_, vt_value, PROP_INTEGER);
}

static void
parse_put_indexed_boolean (const scr_char *list_key,
                           scr_int index_, scr_bool value)
{
  scr_vartype_t vt_value;

  vt_value.boolean = value;
  parse_put_indexed_property (list_key, index_, vt_value, PROP_BOOLEAN);
}


/*
 * parse_get_global_boolean()
 *
 * Retrieve a boolean property of the game's Globals node directly from the
 * bundle, ignoring the current parse key stack.
 */
scr_bool
parse_get_global_boolean (const scr_char *name)
{
  return prop_get_global_boolean (parse_bundle, name);
}


/*
 * parse_get_top_child_count()
 *
 * Count the entries of one of the bundle's top-level lists -- Objects,
 * Tasks, Rooms and the like -- ignoring the current parse key stack.
 */
scr_int
parse_get_top_child_count (scr_prop_setref_t bundle, const scr_char *name)
{
  scr_vartype_t vt_key;

  vt_key.string = name;
  return prop_get_child_count (bundle, "I<-s", &vt_key);
}


/* Parse error, thrown by parse_taf_fail() and caught only in parse_game(). */
struct parse_taf_error_t { };

/* Pushback line, and pushback requested flag. */
static const scr_char *parse_pushback_line = nullptr;
static scr_bool parse_use_pushback = FALSE;

/*
 * parse_taf_fail()
 *
 * Report a bad or exhausted TAF stream with a stack backtrace, then throw
 * out of the parse to parse_game().  Does not return.
 */
#ifdef _MSC_VER
__declspec (noreturn)
#endif
static void
parse_taf_fail (const scr_char *reason, scr_int line)
#ifdef __GNUC__
  __attribute__ ((__noreturn__))
#endif
  ;

static void
parse_taf_fail (const scr_char *reason, scr_int line)
{
  scr_error ("%s at line %ld\n", reason, line);
  parse_stack_backtrace ();
  throw parse_taf_error_t ();
}


/*
 * parse_get_taf_string()
 * parse_get_taf_integer()
 * parse_get_taf_boolean()
 * parse_taf_pushback()
 *
 * Wrapper round obtaining the next TAF file line, with variants to convert
 * the line content into an integer or boolean, and a function for effective
 * TAF line pushback.
 */
static const scr_char *
parse_get_taf_string (void)
{
  const scr_char *line;

  /* If pushback requested, use that instead of reading. */
  if (parse_use_pushback)
    {
      /* Use the pushback line, and clear the request. */
      assert (parse_pushback_line);
      line = parse_pushback_line;
      parse_use_pushback = FALSE;
    }
  else
    {
      /* Get the next line, and complain if absent. */
      line = taf_next_line (parse_taf);
      if (!line)
        parse_taf_fail ("parse_get_taf_string: out of TAF data",
                        parse_tafline);

      /* Note this line for possible pushback. */
      parse_pushback_line = line;
    }

  /* Print out the line we're parsing if tracing. */
  if (parse_trace)
    scr_trace ("Parse: read in line %ld : %s\n", parse_tafline, line);

  parse_tafline++;
  return line;
}

static scr_int
parse_get_taf_integer (void)
{
  const scr_char *line;
  scr_int integer;

  /* Get line, and scan for a single integer; return it. */
  line = parse_get_taf_string ();
  if (sscanf (line, "%ld", &integer) != 1)
    parse_taf_fail ("parse_get_taf_integer: invalid integer",
                    parse_tafline - 1);

  return integer;
}

static scr_bool
parse_get_taf_boolean (void)
{
  const scr_char *line;
  scr_uint boolean;

  /*
   * Get line, and scan for a single integer; check it's a valid-looking flag,
   * and return it.
   */
  line = parse_get_taf_string ();
  if (sscanf (line, "%lu", &boolean) != 1)
    parse_taf_fail ("parse_get_taf_boolean: invalid boolean",
                    parse_tafline - 1);
  if (boolean != 0 && boolean != 1)
    {
      scr_error ("parse_get_taf_boolean:"
                " warning: suspect boolean at line %ld\n", parse_tafline - 1);
    }

  return boolean != 0;
}

static void
parse_taf_pushback (void)
{
  if (parse_use_pushback || !parse_pushback_line)
    scr_fatal ("parse_taf_pushback: too much pushback requested\n");

  /* Set pushback request, and decrement line counter. */
  parse_use_pushback = TRUE;
  parse_tafline--;

  /* Note pushback for tracing purposes. */
  if (parse_trace)
    scr_trace ("Parse: push back at line %ld\n", parse_tafline);
}


/* Enumerations of parse types found in the parse schema. */
enum
{ PARSE_INTEGER = '#',
  PARSE_DEFAULT_ZERO = 'Z',
  PARSE_BOOLEAN = 'B',
  PARSE_DEFAULT_FALSE = 'F',
  PARSE_DEFAULT_TRUE = 'T',
  PARSE_STRING = '$',
  PARSE_DEFAULT_EMPTY = 'E',
  PARSE_MULTILINE = 'M',
  PARSE_VECTOR = 'V',
  PARSE_VECTOR_ALTERNATE = 'W',
  PARSE_ARRAY = '[',
  PARSE_EXPRESSION = '?',
  PARSE_EXPRESSION_NOT = '!',
  PARSE_GLOBAL_EXPRESSION = 'G',
  PARSE_CLASS = '<',
  PARSE_FIXUP = '|',
  PARSE_SPECIAL = '{',
  PARSE_IGNORE_INTEGER = 'i',
  PARSE_IGNORE_BOOLEAN = 'b',
  PARSE_IGNORE_STRING = 's'
};

/* Forward declarations of parse functions for recursion; parse_descriptor()
   is already declared in sctafpar_internal.h. */
static void parse_element (const scr_char *element);
static void parse_class (const scr_char *class_);

/*
 * parse_element_list()
 *
 * Split a list of descriptor elements on the given separator characters,
 * and parse each element in turn.  Both a class descriptor (space-separated)
 * and the tail of a conditional expression (comma-separated) are such lists.
 */
static void
parse_element_list (const scr_char *list, const scr_char *separators)
{
  scr_int next;

  for (next = 0; list[next] != NUL; )
    {
      scr_char element[PARSE_TEMP_LENGTH];
      size_t length;

      /* Isolate the next element of the list. */
      length = strcspn (list + next, separators);
      if (length == 0 || length >= sizeof (element))
        scr_fatal ("parse_element_list: bad list, %s\n", list + next);
      memcpy (element, list + next, length);
      element[length] = NUL;

      /* Parse this isolated element. */
      parse_element (element);

      /* Advance over the element and any trailing separators. */
      next += length;
      next += strspn (list + next, separators);
    }
}

/*
 * parse_repeat_element()
 *
 * Parse the given element count times, pushing the iteration index as a
 * key on each pass.  Common back end for [] arrays and V/W vectors.
 */
static void
parse_repeat_element (const scr_char *element, scr_int count)
{
  scr_int index_;

  for (index_ = 0; index_ < count; index_++)
    {
      scr_vartype_t vt_key;

      vt_key.integer = index_;
      parse_push_key (vt_key, PROP_KEY_INTEGER);

      parse_element (element);

      parse_pop_key ();
    }
}


/*
 * parse_array()
 *
 * Parse a descriptor [] array.
 */
static void
parse_array (const scr_char *array)
{
  scr_int count;
  scr_char element[PARSE_TEMP_LENGTH];

  if (parse_trace)
    scr_trace ("Parse: entering array %s\n", array);

  /* Find the count of elements in the array, and the element itself. */
  if (sscanf (array, "[%ld]%[^ ]", &count, element) != 2)
    scr_fatal ("parse_array: bad array, %s\n", array);

  parse_repeat_element (element, count);

  if (parse_trace)
    scr_trace ("Parse: leaving array %s\n", array);
}


/*
 * parse_vector()
 * parse_vector_alternate()
 *
 * Parse a variable-length vector of properties, sized by a count from the
 * input file, or by that count plus one for the version < 4 'W' form.
 */
static void
parse_vector (const scr_char *vector)
{
  if (parse_trace)
    scr_trace ("Parse: entering vector %s\n", vector);

  parse_repeat_element (vector + 1, parse_get_taf_integer ());

  if (parse_trace)
    scr_trace ("Parse: leaving vector %s\n", vector);
}

static void
parse_vector_alternate (const scr_char *vector)
{
  if (parse_trace)
    scr_trace ("Parse: entering alternate vector %s\n", vector);

  parse_repeat_element (vector + 1, parse_get_taf_integer () + 1);

  if (parse_trace)
    scr_trace ("Parse: leaving alternate vector %s\n", vector);
}


/*
 * parse_test_expression()
 * parse_expression()
 *
 * Parse a conditional field definition, with runtime test.
 */
static scr_bool
parse_test_expression (const scr_char *test_expression)
{
  scr_char plhs[PARSE_TEMP_LENGTH];
  scr_int rhs;
  scr_bool retval = FALSE;

  /* Identify the type of expression to evaluate. */
  switch (test_expression[0])
    {
    case PARSE_BOOLEAN:
      /* Read boolean property and return its value. */
      retval = parse_get_keyed_boolean (test_expression + 1);
      break;

    case PARSE_INTEGER:
      /* Get the left and right sides of = comparison. */
      if (sscanf (test_expression, "#%[^=]=%ld", plhs, &rhs) != 2)
        {
          scr_fatal ("parse_test_expression: bad = compare, %s\n",
                    test_expression + 1);
        }

      /* Read integer property and return comparison. */
      retval = (parse_get_keyed_integer (plhs) == rhs);
      break;

    case PARSE_STRING:
      /* Read property and return TRUE if not an empty string. */
      retval = !scr_strempty (parse_get_keyed_string (test_expression + 1));
      break;

    case PARSE_GLOBAL_EXPRESSION:
      /* Read the given Global boolean property and return it. */
      retval = parse_get_global_boolean (test_expression + 1);
      break;

    default:
      scr_fatal ("parse_test_expression:"
                " bad expression, %s\n", test_expression + 1);
    }

  if (parse_trace)
    scr_trace ("Parse: expression is %s\n", retval ? "true" : "false");

  return retval;
}

static void
parse_expression (const scr_char *expression)
{
  scr_char test_expression[PARSE_TEMP_LENGTH];
  scr_bool is_present;

  if (parse_trace)
    scr_trace ("Parse: entering expression %s\n", expression);

  /* Isolate the test part of the expression. */
  if (sscanf (expression, "?%[^:]", test_expression) != 1)
    scr_fatal ("parse_expression: bad expression, %s\n", expression);

  /* Handle the remainder of the expression only if test passes. */
  is_present = (test_expression[0] == PARSE_EXPRESSION_NOT)
               ? !parse_test_expression (test_expression + 1)
               : parse_test_expression (test_expression);
  if (is_present)
    {
      /*
       * Following the ':' may be a single element, or a comma-separated list.
       */
      parse_element_list (expression + strlen (test_expression) + 2, ",");
    }

  if (parse_trace)
    scr_trace ("Parse: leaving expression %s\n", expression);
}


/*
 * parse_read_multiline()
 *
 * Helper for parse_terminal(), reads in a multiline string.  The return
 * string is malloc'ed, and the caller needs to handle that.
 */
static scr_char *
parse_read_multiline (void)
{
  const scr_byte *separator = parse_version->separator;
  const scr_char *line;
  size_t used;

  /*
   * Own the growing buffer with RAII: parse_get_taf_string() below throws
   * parse_taf_error_t when the TAF runs short, and the unwind frees it.
   */
  scr_owned_string multiline;

  /* Take a simple copy of the first line. */
  line = parse_get_taf_string ();
  used = strlen (line);
  multiline.reset ((scr_char *) scr_malloc (used + 1));
  memcpy (multiline.get (), line, used + 1);

  /*
   * Now concatenate until separator found.  The separator carries its own
   * terminator, so compare as strings: a three-byte memcmp() of an empty last
   * line reads one byte past the end of its slab.
   */
  line = parse_get_taf_string ();
  while (strcmp (line, (const scr_char *) separator) != 0)
    {
      /* Room for what is there, a newline, this line, and the terminator. */
      size_t size = used + strlen (line) + 2;
      scr_char *grown = (scr_char *) scr_realloc (multiline.get (), size);
      /*
       * scr_realloc has freed the old block and returned the grown one; give
       * up ownership of the (now invalid) old pointer without freeing it, and
       * adopt the grown block.  (On allocation failure scr_realloc throws with
       * the old block intact, so multiline still owns and frees it.)
       */
      multiline.release ();
      multiline.reset (grown);
      snprintf (grown + used, size - used, "\n%s", line);
      used = size - 1;
      line = parse_get_taf_string ();
    }

  return multiline.release ();
}


/*
 * parse_terminal()
 *
 * Common handler for string, integer, boolean, and multiline parse terminals.
 */
static void
parse_terminal (const scr_char *terminal)
{
  scr_vartype_t vt_key, vt_value;

  if (parse_trace)
    scr_trace ("Parse: entering terminal %s\n", terminal);

  /* Push the key string. */
  vt_key.string = terminal + 1;
  parse_push_key (vt_key, PROP_KEY_STRING);

  /* Retrieve, or invent, then store the value. */
  switch (terminal[0])
    {
    case PARSE_INTEGER:
      vt_value.integer = parse_get_taf_integer ();
      parse_put_property (vt_value, PROP_INTEGER);
      break;
    case PARSE_DEFAULT_ZERO:
      vt_value.integer = 0;
      parse_put_property (vt_value, PROP_INTEGER);
      break;

    case PARSE_BOOLEAN:
      vt_value.boolean = parse_get_taf_boolean ();
      parse_put_property (vt_value, PROP_BOOLEAN);
      break;
    case PARSE_DEFAULT_FALSE:
    case PARSE_DEFAULT_TRUE:
      vt_value.boolean = (terminal[0] == PARSE_DEFAULT_TRUE);
      parse_put_property (vt_value, PROP_BOOLEAN);
      break;

    case PARSE_STRING:
      vt_value.string = parse_get_taf_string ();
      parse_put_property (vt_value, PROP_STRING);
      break;
    case PARSE_DEFAULT_EMPTY:
      vt_value.string = "";
      parse_put_property (vt_value, PROP_STRING);
      break;

    case PARSE_MULTILINE:
      /* Assign to and adopt mutable string rather than const string. */
      vt_value.mutable_string = parse_read_multiline ();
      parse_put_property (vt_value, PROP_STRING);

      assert (parse_bundle);
      prop_adopt (parse_bundle, vt_value.mutable_string);
      break;

    case PARSE_IGNORE_INTEGER:
      (void) parse_get_taf_integer ();
      break;
    case PARSE_IGNORE_BOOLEAN:
      (void) parse_get_taf_boolean ();
      break;
    case PARSE_IGNORE_STRING:
      (void) parse_get_taf_string ();
      break;

    default:
      scr_fatal ("parse_terminal: bad type, %c\n", terminal[0]);
    }

  /* Pop terminal key. */
  parse_pop_key ();

  if (parse_trace)
    scr_trace ("Parse: leaving terminal %s\n", terminal);
}


/*
 * Resources table.  This table enables resource offsets to be calculated
 * for the various sound and graphic resources encountered on parsing
 * version 4.0 games.  It's unused if the version is not 4.0.
 */
typedef struct
{
  scr_char *name;
  scr_uint hash;
  scr_int length;
  scr_int offset;
} scr_parse_resource_t;

enum { RESOURCE_GROW_INCREMENT = 32 };
static scr_int parse_resources_length = 0;
static scr_int parse_resources_size = 0;
static scr_parse_resource_t *parse_resources = nullptr;


/*
 * parse_clear_v400_resources_table()
 *
 * Free and clear down the version 4.0 resources table.
 */
static void
parse_clear_v400_resources_table (void)
{
  /* Free allocated memory and return to initial values. */
  if (parse_resources)
    {
      scr_int index_;

      for (index_ = 0; index_ < parse_resources_length; index_++)
        scr_free (parse_resources[index_].name);

      scr_free (parse_resources);
      parse_resources = nullptr;
    }
  parse_resources_length = 0;
  parse_resources_size = 0;
}


/*
 * parse_get_v400_resource_offset()
 *
 * Notes version 4.0 resource names encountered in the parse, and their
 * lengths, and builds up a list of resources with their data offsets.  The
 * function assumes that resources are appended to the TAF file in the
 * order in which they are encountered when reading through the TAF file.
 *
 * A warning -- this function may return a new length.  Resources that
 * have been seen once already carry a negative length instead of their real
 * one; this function needs to handle that.  The caller needs to compare
 * length with real_length to see if that happened.
 *
 * What the negative length is, is a back-reference: -N means "this is entry
 * N of the game's resource table", counting from one.  That table holds each
 * distinct resource *name* in the order the parse meets it, with the trailing
 * "##" looping flag stripped before the comparison, and it counts entries
 * whose length is zero -- resources named but not embedded -- just the same
 * as embedded ones.  Only a slot with no name at all is passed over.  The
 * rule reproduces all 535 negative records in the 427-game version 4.0
 * corpus exactly, and the "##" stripping is not cosmetic: To_Hell_And_Beyond
 * has 21 records that only line up once it is done
 * (test/adrift4/notes/RUNNER_TESTS_TODO.md section 9).
 *
 * We resolve back-references by name rather than by index, which comes to
 * the same thing and needs no separate count.  The one case where the
 * distinction bites is a back-reference to a name that was only ever seen
 * with a zero length -- a resource the author referred to but never embedded.
 * Those names are not in our table, since the caller drops zero-length slots
 * before we see them, so the lookup fails; see below for why that must not
 * become a table entry.
 */
static scr_int
parse_get_v400_resource_offset (const scr_char *name,
                                scr_int length, scr_int *real_length)
{
  scr_uint hash;
  scr_int index_, offset;

  /*
   * Take a copy of the name, and remove any trailing "##" looping sound
   * indicator flag.  Who thinks this junk up?  The copy is owned by RAII
   * until it is adopted into the table below, so the scr_realloc of that
   * table throwing (scr_fatal) does not leak it.
   */
  scr_owned_string owned_name (scr_strdup (name));
  scr_char *const clean_name = owned_name.get ();
  if (strlen (clean_name) >= 2
      && strcmp (clean_name + strlen (clean_name) - 2, "##") == 0)
    clean_name[strlen (clean_name) - 2] = NUL;

  /*
   * Scan the current resources list for a matching name, and if the resource
   * is already known, return its offset.  The hash check is an attempt to
   * improve the search times, relative to using only string comparisons --
   * the table's not fully hashed.  If found, we need to also pass back the
   * corrected length.
   */
  offset = -1;
  hash = scr_hash (clean_name);
  for (index_ = 0; index_ < parse_resources_length; index_++)
    {
      if (parse_resources[index_].hash == hash
          && strcmp (parse_resources[index_].name, clean_name) == 0)
        {
          offset = parse_resources[index_].offset;
          break;
        }
    }
  if (offset != -1)
    {
      *real_length = parse_resources[index_].length;
      return offset;
    }

  /*
   * A back-reference we could not resolve names a resource that is not in the
   * TAF: every embedded resource enters the table at its first appearance, so
   * the only way to miss is for that first appearance to have had a length of
   * zero.  Report no data instead of adding an entry, because the entry would
   * hold the back-reference itself as a length, and the offset of every
   * resource added after it is measured from the one before -- a negative
   * length there runs the whole chain backwards.  That was live: it put
   * House's Atmos1.wav 8 bytes early and all ten of the resources
   * MikeDesert_SuburbanProdigy3 embeds after its dangling ".\gold.jpg" 27
   * bytes early, both endings' pictures among them.
   */
  if (length < 0)
    {
      if (parse_trace)
        scr_trace ("Parse: dangling back-reference %ld for \"%s\"\n",
                   length, clean_name);

      *real_length = 0;
      return 0;
    }

  /* Resize the resources table if required. */
  if (parse_resources_length == parse_resources_size)
    {
      parse_resources_size += RESOURCE_GROW_INCREMENT;
      parse_resources = (scr_parse_resource_t *)
        scr_realloc (parse_resources,
                     parse_checked_multiply (parse_resources_size,
                                             sizeof (parse_resources[0])));
    }

  /*
   * Calculate the offset.  For the first resource, it's zero; for others,
   * it's one after the prior entry's offset and length.
   */
  if (parse_resources_length == 0)
    offset = 0;
  else
    {
      offset = parse_resources[parse_resources_length - 1].offset
               + parse_resources[parse_resources_length - 1].length + 1;
    }

  /* Add details to the table, which now owns the name. */
  parse_resources[parse_resources_length].name = owned_name.release ();
  parse_resources[parse_resources_length].hash = hash;
  parse_resources[parse_resources_length].offset = offset;
  parse_resources[parse_resources_length].length = length;
  parse_resources_length++;

  *real_length = length;
  return offset;
}


/*
 * parse_handle_v400_resource()
 *
 * Extra special handling for a version 4.0 resource; extracts details of
 * the sound or graphic just parsed, and adds an offset property if defined.
 *
 * Adrift uses -ve numbers as lengths for resources already parsed, where TAF
 * files include the resource: -N means "see entry N of the resource table",
 * counting from one over distinct names in encounter order.  We ignore the
 * number and work off the resource file name given, which reaches the same
 * entry; parse_get_v400_resource_offset() has the details, and the corpus
 * evidence, above.  This is why we have to look for length not equal to
 * zero, not just lengths greater than zero.
 *
 * Zero-length slots we drop here still occupy a numbered entry in Adrift's
 * table.  That costs us nothing, since we never count -- but it does mean a
 * back-reference can name a resource we have never recorded, and that case
 * is handled where the lookup fails rather than here.
 */
static void
parse_handle_v400_resource (const scr_char *file_key,
                            const scr_char *length_key,
                            const scr_char *offset_key,
                            const scr_char *embedded_key)
{
  const scr_char *file;
  scr_int length;

  /* Retrieve the file and length information for the resource just parsed. */
  file = parse_get_keyed_string (file_key);
  length = parse_get_keyed_integer (length_key);

  /*
   * Note a resource that carries its own data -- a name and a positive
   * length, as the .taf has them -- before the back-reference rewrite below
   * turns every negative length positive.  run400 extracts exactly these to
   * a temp file at load, naming each with a Rnd draw (mdlSpreadTheLoad
   * 454874); run_runner_load_draws() replays those draws.
   */
  parse_put_keyed_integer (embedded_key,
                           (!scr_strempty (file) && length > 0) ? 1 : 0);

  /*
   * Named but with no data of its own.  We drop it below; Adrift still counts
   * it, so trace it -- a back-reference that lands here is why one can dangle.
   */
  if (parse_trace && length == 0
      && !scr_strempty (file) && strcmp (file, "##") != 0)
    scr_trace ("Parse: unembedded resource \"%s\"\n", file);

  /*
   * If defined and has a length, rewrite the offset, and also the length
   * in case changed.
   */
  if (!scr_strempty (file) && length != 0)
    {
      scr_int real_length, offset;

      offset = parse_get_v400_resource_offset (file, length, &real_length);
      parse_put_keyed_integer (offset_key, offset);

      if (parse_trace)
        scr_trace ("Parse: resource \"%s\" len %ld at %ld\n",
                   file, real_length, offset);

      /* Rewrite length if changed. */
      if (real_length != length)
        parse_put_keyed_integer (length_key, real_length);
    }
}


/*
 * parse_optional_exit_record()
 *
 * Helper for parse_special(); parse an optional room exit record.  The next
 * TAF integer is a flag, and when it is nonzero the exit is present and the
 * flag is also its first field, so push it back and parse the record.
 */
static void
parse_optional_exit_record (const scr_char *descriptor)
{
  scr_int flag;

  /* Get next flag, and if true, pushback and parse. */
  flag = parse_get_taf_integer ();
  if (flag != 0)
    {
      parse_taf_pushback ();
      parse_descriptor (descriptor);
    }
}


/*
 * parse_special()
 *
 * Handler for special items that can't be described accurately, and
 * therefore need careful treatment.
 */
static void
parse_special (const scr_char *special)
{
  if (parse_trace)
    scr_trace ("Parse: entering special %s\n", special);

  /* Special handling for version 4.0 resources. */
  if (strcmp (special, "{V400_RESOURCE}") == 0)
    {
      /* Handle whichever resources the global flags say are in use. */
      if (parse_get_global_boolean ("Sound"))
        parse_handle_v400_resource ("SoundFile", "SoundLen", "SoundOffset",
                                    "SoundEmbedded");
      if (parse_get_global_boolean ("Graphics"))
        parse_handle_v400_resource ("GraphicFile", "GraphicLen",
                                    "GraphicOffset", "GraphicEmbedded");
    }

  /* Parse a version 4.0 optional set of room exit information. */
  else if (strcmp (special, V400_ROOM_EXIT_DESC) == 0)
    parse_optional_exit_record ("#Dest #Var1 #Var2 #Var3");

  /* Parse version 3.9 and earlier optional room exit information. */
  else if (strcmp (special, V390_V380_ROOM_EXIT_DESC) == 0)
    parse_optional_exit_record ("#Dest #Var1 #Var2 ZVar3");

  /* Parse room lists, with optional extra room. */
  else if (strcmp (special, "{ROOM_LIST0}") == 0
           || strcmp (special, "{ROOM_LIST1}") == 0)
    {
      scr_int room_count, num_rooms, type, index_;

      /* Retrieve the room list type. */
      type = parse_get_keyed_integer ("Type");

      /* Write remaining room list depending on the type. */
      switch (type)
        {
        case ROOMLIST_NO_ROOMS:
        case ROOMLIST_ALL_ROOMS:
        case ROOMLIST_NPC_PART:
          break;

        case ROOMLIST_ONE_ROOM:
          /* Store this room as the single list entry. */
          parse_element ("#Room");
          break;

        case ROOMLIST_SOME_ROOMS:
          /* Get count of rooms defined, add one if necessary. */
          room_count = parse_get_top_child_count (parse_bundle, "Rooms");

          if (strcmp (special, "{ROOM_LIST1}") == 0)
            num_rooms = room_count + 1;
          else
            num_rooms = room_count;

          /* Store an array of rooms flags for each room. */
          for (index_ = 0; index_ < num_rooms; index_++)
            parse_put_indexed_boolean ("Rooms", index_,
                                       parse_get_taf_boolean ());
          break;

        default:
          scr_fatal ("parse_special: bad type, %ld\n", type);
        }
    }

  /* Parse Parent number iff this object is an NPC part. */
  else if (strcmp (special, "{OBJECT:#Parent}") == 0)
    {
      scr_vartype_t vt_key;
      scr_int type;

      /* Check object's Where room list Type for NPC part. */
      vt_key.string = "Where";
      parse_push_key (vt_key, PROP_KEY_STRING);
      type = parse_get_keyed_integer ("Type");
      parse_pop_key ();

      /* Get Parent if the object is part of an NPC. */
      if (type == ROOMLIST_NPC_PART)
        parse_element ("#Parent");
    }

  /* Parse a list of rooms and times for a walk. */
  else if (strcmp (special, "{WALK:#Rooms_#Times}") == 0)
    {
      scr_int num_stops, index_;

      /* Obtain the count of stops in this walk. */
      num_stops = parse_get_keyed_integer ("NumStops");

      /* Look for a room and time for each stop. */
      for (index_ = 0; index_ < num_stops; index_++)
        {
          parse_put_indexed_integer ("Rooms", index_,
                                     parse_get_taf_integer ());
          parse_put_indexed_integer ("Times", index_,
                                     parse_get_taf_integer ());
        }
    }

  /* Parse a room group variable size boolean list. */
  else if (strcmp (special, "{ROOM_GROUP:[]BList}") == 0)
    {
      scr_vartype_t vt_key;
      scr_int num_rooms, index_, l2index_;
      scr_bool in_group;

      /* Get the count of rooms defined. */
      vt_key.string = "Rooms";
      num_rooms = prop_get_integer (parse_bundle, "I<-s", &vt_key);

      /* Read a boolean for each room. */
      l2index_ = 0;
      for (index_ = 0; index_ < num_rooms; index_++)
        {
          in_group = parse_get_taf_boolean ();

          /* Store raw flag as List[index_]. */
          parse_put_indexed_boolean ("List", index_, in_group);

          /* Store in-group index'es as List2[0..n]. */
          if (in_group)
            parse_put_indexed_integer ("List2", l2index_++, index_);
        }
    }

  /* Error if no special handler available. */
  else
    {
      scr_fatal ("parse_special: no handler for \"%s\"\n", special);
    }

  if (parse_trace)
    scr_trace ("Parse: leaving special %s\n", special);
}


/*
 * parse_element()
 *
 * Parse a class descriptor element.
 */
static void
parse_element (const scr_char *element)
{
  if (parse_trace)
    scr_trace ("Parse: entering element %s\n", element);

  /* Determine the element type from the first character. */
  switch (element[0])
    {
    case PARSE_ARRAY:
      parse_array (element);
      break;
    case PARSE_VECTOR:
      parse_vector (element);
      break;
    case PARSE_VECTOR_ALTERNATE:
      parse_vector_alternate (element);
      break;
    case PARSE_CLASS:
      parse_class (element);
      break;
    case PARSE_EXPRESSION:
      parse_expression (element);
      break;
    case PARSE_SPECIAL:
      parse_special (element);
      break;
    case PARSE_FIXUP:
      /* Hand fixups to the handler specific to the TAF file version. */
      parse_version->fixup (element);
      break;

    case PARSE_INTEGER:
    case PARSE_DEFAULT_ZERO:
    case PARSE_BOOLEAN:
    case PARSE_DEFAULT_TRUE:
    case PARSE_DEFAULT_FALSE:
    case PARSE_STRING:
    case PARSE_DEFAULT_EMPTY:
    case PARSE_IGNORE_INTEGER:
    case PARSE_IGNORE_BOOLEAN:
    case PARSE_IGNORE_STRING:
    case PARSE_MULTILINE:
      parse_terminal (element);
      break;
    default:
      scr_fatal ("parse_element: bad type, %c\n", element[0]);
    }

  if (parse_trace)
    scr_trace ("Parse: leaving element %s\n", element);
}


/*
 * parse_descriptor()
 *
 * Parse a class's properties descriptor list.
 */
void
parse_descriptor (const scr_char *descriptor)
{
  parse_element_list (descriptor, " ");
}


/*
 * parse_class()
 *
 * Parse a class of properties.
 */
static void
parse_class (const scr_char *class_)
{
  const scr_parse_schema_t *const schema = parse_version->schema;
  scr_char class_name[PARSE_TEMP_LENGTH];
  scr_int index_;
  scr_vartype_t vt_key;

  /* Isolate the class name. */
  if (sscanf (class_, "<%[^>]", class_name) != 1)
    scr_fatal ("parse_class: error in class, %s\n", class_);
  if (parse_trace)
    scr_trace ("Parse: entering class %s\n", class_name);

  /* Find the class in the parse schema, and fail if not found. */
  for (index_ = 0; schema[index_].class_name; index_++)
    {
      if (strcmp (schema[index_].class_name, class_name) == 0)
        break;
    }
  if (!schema[index_].class_name)
    scr_fatal ("parse_class: class not described, %s\n", class_name);

  /*
   * Unless we are at the top level of the parse schema, push the class tag
   * as a key.  The top level is "_GAME_", index_ 0, and isn't part of key
   * formation.
   */
  if (index_ > 0)
    {
      vt_key.string = class_ + strlen (class_name) + 2;
      parse_push_key (vt_key, PROP_KEY_STRING);
    }

  /* Parse each element in the descriptor. */
  parse_descriptor (schema[index_].descriptor);

  /* Pop a key if the class tag was pushed above. */
  if (index_ > 0)
    parse_pop_key ();

  if (parse_trace)
    scr_trace ("Parse: leaving class %s\n", class_name);
}


/*
 * parse_trim_object_names()
 *
 * Normalize object Prefix and Short exactly as the Runner's loader does, as
 * post-processing after the TAF file has been successfully parsed.
 *
 * run400 reads the two fields back to back in mdlSpreadTheLoad.bas:7594-7653.
 * After LineInput'ing Prefix (loc_4900E3) -- and substituting the literal "a"
 * for an empty one (loc_4900EC) -- it loops
 *
 *   loc_490100..loc_49015C:  If Right(s, 1) = " " Then s = Left(s, Len(s) - 1)
 *
 * stripping every TRAILING space from the prefix; then, having LineInput'ed
 * Short (loc_49016C), it loops
 *
 *   loc_490170..loc_4901CC:  If Left(s, 1) = " " Then s = Right(s, Len(s) - 1)
 *
 * stripping every LEADING space from the short name.  run370 (@43F5DA) and
 * run380 (@4481B2) carry the same pair of loops, so this is not version-split.
 *
 * The trim belongs in the loader, not in the printers: every later consumer --
 * the name builder that joins Prefix, " " and Short, the room and container
 * listings, and the noun matcher that has to recognize what it printed -- sees
 * the trimmed text in the Runner, so they all see it here too.
 *
 * Fourteen objects across nine corpus games are written with the spaces still
 * on, among them Crime_Adventure's "an arcade token ", arlo's "the ", and
 * ADRIFTMAS_Party's " bathroom door".
 *
 * The empty-Prefix substitution belongs here too, and it happens BEFORE the
 * trailing-space strip, so the two are not interchangeable: an authored ""
 * becomes the literal "a", while an authored " " is not empty, escapes the
 * substitution, and is then trimmed to a genuinely empty prefix that no later
 * code puts an article back into.  Both halves are measured -- probe ISARE
 * (run400, runner_probes/isare.run400.txt, 2026-09-07) lists its no-prefix
 * boots as "a boots" and its single-space rings as " rings", with the joining
 * space and no article, and answers `where rings` with " rings are test
 * arena."
 *
 * Doing it in the loader retires three downstream imitations of it (the
 * "the "/"a " empty-prefix defaults in lib_print_object_np, lib_print_object
 * and lib_print_object_raw, and the pronoun antecedent's own copy in
 * scparser.cpp), and it also feeds the parser and the %objectN% expansions,
 * which in the Runner see the substituted field like everything else.  No
 * corpus game authors a whitespace-only object Prefix -- 27386 objects over
 * 426 games, 2844 of them exactly empty and none whitespace-only -- so the
 * second half of this is faithfulness, not a fix for a live game.
 */
static void
parse_trim_object_names (scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[3];
  scr_int objects_count, object;

  /* Get the count of objects. */
  vt_key[0].string = "Objects";
  objects_count = prop_get_child_count (bundle, "I<-s", vt_key);

  for (object = 0; object < objects_count; object++)
    {
      const scr_char *prefix, *short_name;
      scr_int length;

      vt_key[1].integer = object;

      /* Substitute "a" for an empty prefix, then strip its trailing spaces. */
      vt_key[2].string = "Prefix";
      prefix = prop_get_string (bundle, "S<-sis", vt_key);
      if (prefix[0] == NUL)
        {
          prop_put_string (bundle, "S<-sis", "a", vt_key);
          prefix = prop_get_string (bundle, "S<-sis", vt_key);
        }
      length = strlen (prefix);
      while (length > 0 && prefix[length - 1] == ' ')
        length--;
      if (prefix[length] != NUL)
        {
          scr_owned_string trimmed ((scr_char *) scr_malloc (length + 1));

          memcpy (trimmed.get (), prefix, length);
          trimmed.get ()[length] = NUL;
          prop_put_string (bundle, "S<-sis", trimmed.get (), vt_key);
        }

      /* Strip leading spaces from the short name. */
      vt_key[2].string = "Short";
      short_name = prop_get_string (bundle, "S<-sis", vt_key);
      if (short_name[0] == ' ')
        {
          const scr_char *trimmed = short_name;

          while (*trimmed == ' ')
            trimmed++;
          prop_put_string (bundle, "S<-sis", trimmed, vt_key);
        }
    }
}


/*
 * parse_add_walkalerts()
 *
 * Add a list of all NPC walks started by each task.  This is post-processing
 * that occurs after the TAF file has been successfully parsed.
 */
static void
parse_add_walkalerts (scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[5];
  scr_int npcs_count, npc, task_count;

  /* Get the count of tasks, and of NPCs. */
  task_count = parse_get_top_child_count (bundle, "Tasks");
  npcs_count = parse_get_top_child_count (bundle, "NPCs");
  vt_key[0].string = "NPCs";

  /* Set up each NPC. */
  for (npc = 0; npc < npcs_count; npc++)
    {
      scr_int walk_count, walk;

      /* Get NPC walk details. */
      vt_key[1].integer = npc;
      vt_key[2].string = "Walks";
      walk_count = prop_get_child_count (bundle, "I<-sis", vt_key);

      for (walk = 0; walk < walk_count; walk++)
        {
          scr_int starttask;

          /*
           * Get start task of walk.  One beyond the tasks starts no walk;
           * putting an alert under it would invent a task.
           */
          vt_key[3].integer = walk;
          vt_key[4].string = "StartTask";
          starttask = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
          if (starttask >= 0 && starttask < task_count)
            {
              scr_vartype_t vt_key2[4], vt_value;
              scr_int count;

              /* Count existing walkalerts for the task. */
              vt_key2[0].string = "Tasks";
              vt_key2[1].integer = starttask;
              vt_key2[2].string = "NPCWalkAlert";
              count = prop_get_child_count (bundle, "I<-sis", vt_key2);

              /* Add two more -- NPC and walk. */
              vt_key2[3].integer = count;
              vt_value.integer = npc;
              prop_put (bundle, "I->sisi", vt_value, vt_key2);
              vt_key2[3].integer = count + 1;
              vt_value.integer = walk;
              prop_put (bundle, "I->sisi", vt_value, vt_key2);
            }
        }
    }
}


/*
 * parse_add_movetimes()
 *
 * Add a list of move times to all NPC walks.  This is post-processing that
 * occurs after the TAF file has been successfully parsed.
 */
static void
parse_add_movetimes (scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[6];
  scr_int npcs_count, npc;

  /* Get the count of NPCs. */
  vt_key[0].string = "NPCs";
  npcs_count = prop_get_child_count (bundle, "I<-s", vt_key);

  /* Set up each NPC. */
  for (npc = 0; npc < npcs_count; npc++)
    {
      scr_int walk_count, walk;

      /* Get NPC walk details. */
      vt_key[1].integer = npc;
      vt_key[2].string = "Walks";
      walk_count = prop_get_child_count (bundle, "I<-sis", vt_key);

      for (walk = 0; walk < walk_count; walk++)
        {
          scr_int waittimes, index_;
          scr_vartype_t vt_value;

          vt_key[3].integer = walk;
          vt_key[4].string = "Times";
          waittimes = prop_get_child_count (bundle, "I<-sisis", vt_key);

          /*
           * Value-initialized to zero; frees itself if prop_*() throws.  A
           * negative waittimes would size the vector to zero yet still be
           * used as an index at movetimes[waittimes] below, so validate it.
           */
          std::vector<scr_int> movetimes (parse_checked_count (waittimes) + 1);
          for (index_ = waittimes - 1; index_ >= 0; index_--)
            {
              vt_key[4].string = "Times";
              vt_key[5].integer = index_;
              movetimes[index_] = prop_get_integer (bundle, "I<-sisisi", vt_key)
                                  + movetimes[index_ + 1];
            }
          movetimes[waittimes] = -2;

          for (index_ = 0; index_ <= waittimes; index_++)
            {
              vt_key[4].string = "MoveTimes";
              vt_key[5].integer = index_;
              vt_value.integer = movetimes[index_];
              prop_put (bundle, "I->sisisi", vt_value, vt_key);
            }
        }
    }
}


/*
 * parse_add_alrs_index()
 *
 * Sort ALRs by original string length and store an indexer property, so
 * that ALR replacements look at longer strings before shorter ones.
 */
static void
parse_add_alrs_index (scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[3];
  scr_int alr_count, index_, alr;
  scr_int longest, shortest, length;

  /* Count ALRs, and set invariant part of properties key. */
  vt_key[0].string = "ALRs";
  alr_count = prop_get_child_count (bundle, "I<-s", vt_key);

  /*
   * Set up an array of the lengths of ALR original strings, and while at it,
   * get the shortest and longest defined.  A std::vector frees itself if a
   * prop_*() call below throws (scr_fatal) mid-parse.
   */
  std::vector<scr_int> alr_lengths (parse_checked_count (alr_count));
  shortest = INT_MAX;
  longest = 0;
  for (index_ = 0; index_ < alr_count; index_++)
    {
      const scr_char *original;

      original = prop_get_indexed_string (bundle, "ALRs", index_, "Original");
      length = strlen (original);

      alr_lengths[index_] = length;
      shortest = (length < shortest) ? length : shortest;
      longest = (length > longest) ? length : longest;
    }

  /*
   * Now write a set of secondary properties that define the order of handling
   * for ALRs.  Our friend qsort() can't help here as it doesn't define the
   * final ordering of equal members, and we need here to retain file ordering
   * for ALR originals of the same length.
   */
  vt_key[0].string = "ALRs2";
  alr = 0;
  for (length = longest; length >= shortest; length--)
    {
      /* Find and add each ALR of this length. */
      for (index_ = 0; index_ < alr_count; index_++)
        {
          if (alr_lengths[index_] == length)
            {
              scr_vartype_t vt_value;

              vt_key[1].integer = alr++;
              vt_key[2].string = "ALRIndex";
              vt_value.integer = index_;
              prop_put (bundle, "I->sis", vt_value, vt_key);
            }
        }
    }
  assert (alr == alr_count);
}


/*
 * parse_add_resources_offset()
 *
 * Add the resources offset to the properties as an extra game property
 * for version 4.0 games.  For earlier versions, write zero; only version 4.0
 * games can embed their resources into the TAF file.
 */
static void
parse_add_resources_offset (scr_prop_setref_t bundle, scr_tafref_t taf)
{
  scr_vartype_t vt_key, vt_value;
  scr_bool embedded;
  scr_int offset;

  /*
   * Get the resources offset from the TAF, or default to zero.  The resources
   * offset is one byte after the end of game data.
   */
  embedded = prop_get_global_boolean (bundle, "Embedded");
  offset = embedded ? taf_get_game_data_length (taf) + 1 : 0;

  if (parse_trace)
    scr_trace ("Parse: resource base %ld\n", offset);

  /* Add this offset to the properties. */
  vt_key.string = "ResourceOffset";
  vt_value.integer = offset;
  prop_put (bundle, "I->s", vt_value, &vt_key);
}


/*
 * parse_add_version()
 *
 * Add the TAF version to the properties, both integer and character forms
 * for convenience.
 */
static void
parse_add_version (scr_prop_setref_t bundle, scr_tafref_t taf)
{
  scr_vartype_t vt_key, vt_value;

  /* Add the version integer to the properties. */
  vt_key.string = "Version";
  vt_value.integer = taf_get_version (taf);
  prop_put (bundle, "I->s", vt_value, &vt_key);

  /* Add the version string to the properties. */
  vt_value.string = parse_version->version_string;
  vt_key.string = "VersionString";
  prop_put (bundle, "S->s", vt_value, &vt_key);
}


/*
 * parse_reset()
 *
 * Forget the TAF, the bundle and the parse state once a parse is over,
 * whether or not it succeeded, and free the version 4.0 resources table.
 */
static void
parse_reset (void)
{
  parse_clear_v400_resources_table ();
  parse_taf = nullptr;
  parse_bundle = nullptr;
  parse_version = nullptr;
  parse_depth = 0;
  parse_pushback_line = nullptr;
  parse_use_pushback = FALSE;
}


/*
 * parse_game()
 *
 * Parse a game into a set properties.  Return TRUE on success, FALSE if
 * it encountered an error reading the TAF file.
 */
scr_bool
parse_game (scr_tafref_t taf, scr_prop_setref_t bundle)
{
  assert (taf && bundle);

  /* Store the TAF to read from, and the bundle to store into. */
  parse_taf = taf;
  parse_bundle = bundle;
  parse_version = parse_select_version (parse_taf);
  parse_depth = 0;

  /* Try parsing, and catch errors thrown by parse_taf_fail(). */
  try
    {
      /* Parse a complete game. */
      taf_first_line (parse_taf);
      parse_tafline = 0;
      parse_class ("<_GAME_>");
    }
  catch (const parse_taf_error_t &)
    {
      /* Error with one of the TAF file lines. */
      parse_reset ();
      return FALSE;
    }

  /* See if we reached the end of the TAF. */
  if (taf_more_lines (parse_taf))
    scr_error ("parse_game: unexpected trailing data\n");

  /* Trim object names as the loader does, then append post-processing
     walkalerts and move times. */
  parse_trim_object_names (parse_bundle);
  parse_add_walkalerts (parse_bundle);
  parse_add_movetimes (parse_bundle);

  /* Append sorted ALR list and resources offset. */
  parse_add_alrs_index (parse_bundle);
  parse_add_resources_offset (parse_bundle, parse_taf);

  /* Add a note of the TAF file version. */
  parse_add_version (parse_bundle, parse_taf);

  /* Correct the few games with a data bug that makes them unwinnable; a
     no-op unless patching is on and this is one of them.  Last, because it
     wants the game complete, and before solidify, which freezes it. */
  parse_apply_game_patches (parse_bundle);

  /* Trim excess allocations from properties. */
  prop_solidify (parse_bundle);

  parse_reset ();
  return TRUE;
}


/*
 * parse_debug_trace()
 *
 * Set parse tracing on/off.
 */
void
parse_debug_trace (scr_bool flag)
{
  parse_trace = flag;
}
