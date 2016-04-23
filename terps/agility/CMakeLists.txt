set(srcs agtread.c gamedata.c util.c agxfile.c auxfile.c filename.c parser.c exec.c runverb.c metacommand.c savegame.c debugcmd.c agil.c token.c disassemble.c object.c interface.c os_glk.c)

interpreter(agility "${srcs}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGLK -DGLK_ANSI_ONLY")
