ifdef COMSPEC
DOTEXE:=.exe
else
DOTEXE:=
endif



########### variables


ROMNAME := DiGi Charat - DigiCommunication (J) [!].gba

CFLAGS :=-flto -Ofast -Wall -I .
LDFLAGS:=-s
LDLIBS := -lpng -lz -lm


TOOLS:=extract build
TOOL_SRC:=$(foreach TOOL,$(TOOLS),src/$(TOOL).c)
TOOL_DEP:=$(foreach TOOL,$(TOOLS),dep/$(TOOL).dep)
TOOL_OBJ:=$(foreach TOOL,$(TOOLS),obj/$(TOOL).obj)
TOOL_EXE:=$(foreach TOOL,$(TOOLS),bin/$(TOOL).exe)

LIBS:=compress decompress lex text-extract lex.yy
LIB_SRC:=$(foreach LIB,$(LIBS),src/$(LIB).c)
LIB_DEP:=$(foreach LIB,$(LIBS),dep/$(LIB).dep)
LIB_OBJ:=$(foreach LIB,$(LIBS),obj/$(LIB).obj)



HACKS := hacks/hacks.asm
HACKS_OUT := hacks/hacks.gba




########## phony targets


.PHONY: default all tools hacks clean

default: t.gba

all: tools hacks t.gba

tools: $(TOOL_EXE)
hacks: $(HACKS_OUT)


clean:
	$(RM) $(TOOL_DEP) $(LIB_DEP)
	$(RM) $(TOOL_OBJ) $(LIB_OBJ)
	$(RM) $(TOOL_EXE)
	$(RM) $(addprefix src/lex.yy.,c h)
	$(RM) $(HACKS_OUT)
	$(RM) t.gba



########## dependencies


dep/%.dep: src/%.c
	$(CC) -MM -MG -MT obj/$*.obj -MF $@ $<

-include $(TOOL_DEP) $(LIB_DEP)



########## generic targets

obj/%.obj: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

bin/%$(DOTEXE): obj/%.obj $(LIB_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)


%.yy.c %.yy.h: %.l
	$(LEX) -o $*.yy.c --header-file=$*.yy.h $<




########## phony rip targets

RIP_TARGETS:=rip-dsc rip-mbm rip-mcm rip-mfm rip-mrm rip-text
.PHONY: rip-all $(RIP_TARGETS)

rip-all: $(RIP_TARGETS)
$(RIP_TARGETS): bin/extract$(DOTEXE)

rip-dsc:
	mkdir -p orig-data/DSC && bin/extract '$(ROMNAME)' dsc orig-data/DSC
rip-mbm:
	mkdir -p orig-data/MBM && bin/extract '$(ROMNAME)' mbm orig-data/MBM
rip-mcm:
	mkdir -p orig-data/MCM && bin/extract '$(ROMNAME)' mcm orig-data/MCM
rip-mfm:
	mkdir -p orig-data/MFM && bin/extract '$(ROMNAME)' mfm orig-data/MFM
rip-mrm:
	mkdir -p orig-data/MRM && bin/extract '$(ROMNAME)' mrm orig-data/MRM
rip-text: stringlist.txt
	mkdir -p orig-data && bin/extract '$(ROMNAME)' text stringlist.txt orig-data/text.txt




########## main outputs


$(HACKS_OUT): $(HACKS)
	armips -strequ OUTNAME $@ -strequ ROMNAME "$(ROMNAME)" $<


# make dirs in new-data if they don't exist
$(addprefix new-data/,DSC MBM MFM MRM):
	mkdir -p $@

t.gba: bin/build$(DOTEXE) $(HACKS_OUT) $(addprefix new-data/,DSC MBM MFM MRM text.txt)
	$^ $@



######### phony helper targets

.PHONY: hunt-undone-lines
hunt-undone-lines:
	grep -m 1 -rnH '"!!!' new-data/DSC


