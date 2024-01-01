#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

// need to explicitly say "src" so dep/lex.dep won't depend on this in the home directory
#include "src/lex.yy.h"

extern char *lexsrcnam;

char *tokstr(int t);

char* makeprintable(uint8_t v);

void err(const char* fmt, ...);
void err_wrongtype(int good, int bad);

long long stoi(char *s);

int processstring(uint8_t *dest, uint8_t *src);

void lexinit(FILE *f, char *fnam);