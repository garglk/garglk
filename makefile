# top-level build for gargoyle
# type 'make' to build everything and produce a build/dist
DISTDIR=mk/dist
BUILDTYPE?=-O2
TERPS=advsys agility alan2 alan3 bocfel frotz geas git glulxe hugo jacl level9 magnetic nitfol scare scott

all: release
debug: BUILDTYPE=-g -DGARGLK_DEBUG
debug: release

release: babel garglk terps tads

terps: $(TERPS)

$(TERPS): garglk
	$(MAKE) -C terps/$@ $(MAKECMDGOALS)

garglk: babel
	$(MAKE) -C $@ $(MAKECMDGOALS)

tads:
	$(MAKE) -C $@ $(MAKECMDGOALS)

babel:
	$(MAKE) -C support/babel $(MAKECMDGOALS)

dist: release
	mkdir -p $(DISTDIR)
	# TODO copy and chmod (maybe tar)

clean-dist:
	-rm $(DISTDIR)/* 2>/dev/null ; echo >/dev/null

clean: clean-dist terps tads

.PHONY: clean-dist all release debug terps garglk tads dist clean
