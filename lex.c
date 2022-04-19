#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>

#include "lex.h"




char* lexsrcnam = NULL;


char *tokstr(int t)
{
    switch (t)
    {
        case 0:
            return "end of file";
        case TOK_NUM: 
            return "number";
        case TOK_ID:
            return "identifier";
        case TOK_STRING:
            return "string";
        case TOK_INVALID:
            return "bad token";
        default:
            return "error";
    }
    
}

char* makeprintable(uint8_t v)
{
    static char mpbuf[8];
    if (isgraph(v))
        sprintf(mpbuf, "'%c'", v);
    else
        sprintf(mpbuf, "$%02X", v);
    return mpbuf;
}

void err(const char* fmt, ...)
{
    printf("%s", lexsrcnam);
    if (yylineno != -1) printf(" line %i", yylineno);
    printf(": ");
    
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    
    putc('\n', stdout);
}

void err_wrongtype(int good, int bad)
{
    err("expected %s, got %s", tokstr(good), tokstr(bad));
}


long long stoi(char *s)
{
    long long v = 0;
    int si = 0;
    int sign = 1;
    int hex = 0;
    if (s[si] == '-')
    {
        sign = -1;
        si++;
    }
    if (s[si] == '$')
    {
        hex = 1;
        si++;
    }
    if (s[si] == '0' && tolower(s[si+1]) == 'x')
    {
        hex = 1;
        si += 2;
    }
    char c;
    while ((c = tolower(s[si++])))
    {
        uint8_t d = 0;
        if (c >= '0' && c <= '9')
        {
            d = c-'0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            d = c-'a'+0xa;
        }
        v = (v * (hex ? 16 : 10) + d);
    }
    return v * sign;
}


/* returns amount of chars sent to dest including null terminator (-1 if error) */
int processstring(uint8_t *dest, uint8_t *src)
{
    int si = 0;
    int di = 0;
    uint8_t c;
    while ((c = src[si++]))
    {
        if (c == '\\')
        {
            c = src[si++];
            switch (c)
            {
                case 'n':
                    dest[di++] = 0x0a;
                    break;
                case '\\':
                    dest[di++] = '\\';
                    break;
                case '\"':
                    dest[di++] = '\"';
                    break;
                case 'x':
                {
                    uint8_t b = 0;
                    for (int i = 0; i < 2; i++)
                    {
                        c = src[si++];
                        uint8_t d;
                        if (c >= '0' && c <= '9')
                        {
                            d = c-'0';
                        }
                        else if (c >= 'a' && c <= 'f')
                        {
                            d = c-'a'+0xa;
                        }
                        else if (c >= 'A' && c <= 'F')
                        {
                            d = c-'A'+0xa;
                        }
                        else
                        {
                            err("char %i in string: %s is not a hex digit", si, makeprintable(c));
                            return -1;
                        }
                        b |= d << ((1-i)*4);
                    }
                    dest[di++] = b;
                    break;
                }
                default:
                    err("char %i in string: unknown escape code %s", si, makeprintable(c));
                    return -1;
            }
        }
        else if ((c < 0x80) || (c >= 0xa1 && c < 0xe0))
        {
            dest[di++] = c;
        }
        else
        {
            dest[di++] = c;
            dest[di++] = src[si++];
        }
    }
    dest[di++] = 0;
    return di;
}


void lexinit(FILE *f, char *fnam)
{
    yyrestart(f);
    lexsrcnam = fnam;
    yylineno = 1;
}

