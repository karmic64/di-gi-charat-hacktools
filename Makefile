ifdef COMSPEC
DOTEXE:=.exe
else
DOTEXE:=
endif

# note, this makefile only builds the tools!
# please refer to "0GUIDELINES FOR TRANSLATORS.txt" for info on building the translation


# todo: don't hardcode this
ROMNAME := DiGi Charat - DigiCommunication (J) [!].gba

BUILDDATE := $(shell date -u +"%Y/%m/%d")
BUILDTIME := $(shell date -u +"%H:%M:%S")

# static builds for ease of distribution
CFLAGS := -static -s -Ofast -Wall -D ROMNAME="\"$(ROMNAME)\"" -D BUILDDATE="\"$(BUILDDATE)\"" -D BUILDTIME="\"$(BUILDTIME)\"" -I .
LIBS := -lpng -lz

SRCS := build.c dsc.c mbm.c mcm.c mfm.c mrm.c text.c
DGVC_SRCS := $(addprefix dgvc/,dump-dsc.c gen-dsc.c gen-text.c)
ALL_SRCS := $(SRCS) $(DGVC_SRCS)

OUTS := $(SRCS:.c=$(DOTEXE))
DGVC_OUTS := $(DGVC_SRCS:.c=$(DOTEXE))
ALL_OUTS := $(OUTS) $(DGVC_OUTS)

HACKS := hacks/hacks.asm
HACKS_OUT := hacks/hacks.gba




.PHONY: default all tools dgvc-tools hacks clean

default: tools hacks

all: tools dgvc-tools hacks

tools: $(OUTS)
dgvc-tools: $(DGVC_OUTS)
hacks: $(HACKS_OUT)


clean:
	-$(RM) $(ALL_OUTS) $(HACKS_OUT) $(ALL_SRCS:.c=.d) *.yy.c


%.d: %.c
	$(CC) -M -MG -MT $(<:.c=$(DOTEXE)) -MF $@ $<

-include $(ALL_SRCS:.c=.d)



%$(DOTEXE): %.c
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)


%.yy.c: %.l
	$(LEX) -o $@ $<


$(HACKS_OUT): $(HACKS)
	armips -strequ OUTNAME $@ -strequ ROMNAME "$(ROMNAME)" -strequ BUILDDATE "$(BUILDDATE)" -strequ BUILDTIME "$(BUILDTIME)" $<

