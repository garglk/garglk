set(SRCS agtread.c gamedata.c util.c agxfile.c auxfile.c filename.c parser.c exec.c runverb.c metacommand.c savegame.c debugcmd.c agil.c token.c disassemble.c object.c interface.c os_glk.c)

interpreter(agility "${SRCS}")
target_compile_definitions(agility PRIVATE GLK GLK_ANSI_ONLY)
