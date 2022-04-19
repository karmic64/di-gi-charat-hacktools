ifdef COMSPEC
DOTEXE:=.exe
STATIC:=-static
else
DOTEXE:=
STATIC:=
endif

# note, this makefile only builds the tools!
# please refer to "0GUIDELINES FOR TRANSLATORS.txt" for info on building the translation



########### variables


ROMNAME := DiGi Charat - DigiCommunication (J) [!].gba

BUILDDATE := $(shell date -u +"%Y/%m/%d")
BUILDTIME := $(shell date -u +"%H:%M:%S")

CFLAGS := -Ofast -Wall -D BUILDDATE="\"$(BUILDDATE)\"" -D BUILDTIME="\"$(BUILDTIME)\"" -I .
LIBS := -lpng -lz -lm


EXE_SRCS := extract.c build.c
EXES := $(EXE_SRCS:.c=$(DOTEXE))


SRCS := $(EXE_SRCS) compress.c decompress.c lex.c text-extract.c
OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)



HACKS := hacks/hacks.asm
HACKS_OUT := hacks/hacks.gba




########## phony targets


.PHONY: default all tools dgvc-tools hacks clean

default: tools hacks

all: tools dgvc-tools hacks

tools: extract$(DOTEXE) build$(DOTEXE)
dgvc-tools: $(DGVC_OUTS)
hacks: $(HACKS_OUT)


clean:
	-$(RM) $(OBJS) $(DEPS) $(EXES) $(HACKS_OUT) *.yy.*



########## dependencies


%.d: %.c
	$(CC) -M -MG -MT $(<:.c=.o) -MF $@ $<

-include $(DEPS)



########## generic targets

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%$(DOTEXE): %.o
	$(CC) -s -o $@ $^ $(LIBS)


%.yy.c %.yy.h: %.l
	$(LEX) -o $*.yy.c --header-file=$*.yy.h $<




########## executables

extract$(DOTEXE): decompress.o lex.o lex.yy.o text-extract.o

build$(DOTEXE): compress.o lex.o lex.yy.o


$(HACKS_OUT): $(HACKS)
	armips -strequ OUTNAME $@ -strequ ROMNAME "$(ROMNAME)" -strequ BUILDDATE "$(BUILDDATE)" -strequ BUILDTIME "$(BUILDTIME)" $<

