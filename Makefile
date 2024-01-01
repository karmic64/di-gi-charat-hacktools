ifdef COMSPEC
DOTEXE:=.exe
else
DOTEXE:=
endif

# note, this makefile only builds the tools!
# please refer to "0GUIDELINES FOR TRANSLATORS.txt" for info on building the translation



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

default: tools hacks

all: tools hacks

tools: $(TOOL_EXE)
hacks: $(HACKS_OUT)


clean:
	$(RM) $(TOOL_DEP) $(LIB_DEP)
	$(RM) $(TOOL_OBJ) $(LIB_OBJ)
	$(RM) $(TOOL_EXE)
	$(RM) $(addprefix src/lex.yy.,c h)
	$(RM) $(HACKS_OUT)



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




########## executables


$(HACKS_OUT): $(HACKS)
	armips -strequ OUTNAME $@ -strequ ROMNAME "$(ROMNAME)" $<

