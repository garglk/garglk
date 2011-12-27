# Microsoft NMAKE makefile for TADS WebUI flash component
#
# Requires the Flex SDK: http://www.adobe.com/products/flex/
# (Adobe publishes a free, open-source Flex SDK as well as a
# commercial toolkit called Adobe Flash Builder.  The free SDK is
# sufficient for our purposes here.)

FLEXC = c:\downloads\flex\bin\mxmlc
RESDIR = ..\webuires
WWWDIR = c:\website\mjrnet.org

all: $(RESDIR)\TADS.swf $(WWWDIR)\tads-swf.htm $(WWWDIR)\TADS.swf

$(WWWDIR)\tads-swf.htm: tads-swf.htm
    copy /y $? $@

$(WWWDIR)\tads.swf: $(RESDIR)\TADS.swf
    copy /y $? $@

$(RESDIR)\TADS.swf: TADS.as

.SUFFIXES: .swf .as

.as{$(RESDIR)}.swf:
    $(FLEXC) -use-network=false -static-link-runtime-shared-libraries -output $@ $<

