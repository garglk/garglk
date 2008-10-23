# Build sources are in main build and in unit test
BUILDSRCS = \
	rules.c \
	debug.c \
	args.c \
	decode.c \
	term.c \
	readline.c \
	params.c \
	act.c

# Main sources are only in main build because unit #includes them
MAINSRCS = \
	exe.c \
	sysdep.c \
	stack.c \
	inter.c \
	reverse.c \
	syserr.c \
	parse.c \
	main.c \
	set.c \
	arun.c \
	state.c \
	save.c

VERSIONSRCS = $(MAINSRCS) $(BUILDSRCS)
VERSIONOBJECTS = ${VERSIONSRCS:.c=.o}
ARUNOBJECTS = $(VERSIONOBJECTS) alan.version.o
GLKOBJECTS = ${GLKSRCS:.c=.o}
WINARUNOBJECTS = ${GLKOBJECTS} ${ARUNOBJECTS}
TERMARUNOBJECTS = ${GLKOBJECTS} ${ARUNOBJECTS}

# Sources for the test framework
UNITSRCS = unit.c

TESTSRCS = $(UNITSRCS) $(BUILDSRCS)
TESTOBJECTS = ${TESTSRCS:.c=.o} alan.version.o
