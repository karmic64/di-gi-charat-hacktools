#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>

#include "lex.yy.c"


const uint16_t ascjistbl[] = {
/* 20         !      "      #      $      %      &      '   */
    0x8140,0x8149,0x8168,0x8194,0x8190,0x8193,0x8195,0x8166,
/* 28  (      )      *      +      ,      -      .      /   */
    0x8169,0x816a,0x8196,0x817b,0x8143,0x817d,0x8144,0x815e,
/* 30  0      1      2      3      4      5      6      7   */
    0x824f,0x8250,0x8251,0x8252,0x8253,0x8254,0x8255,0x8256,
/* 38  8      9      :      ;      <      =      >      ?   */
    0x8257,0x8258,0x8146,0x8147,0x8183,0x8181,0x8184,0x8148,
/* 40  @      A      B      C      D      E      F      G   */
    0x8197,0x8260,0x8261,0x8262,0x8263,0x8264,0x8265,0x8266,
/* 48  H      I      J      K      L      M      N      O   */
    0x8267,0x8268,0x8269,0x826a,0x826b,0x826c,0x826d,0x826e,
/* 50  P      Q      R      S      T      U      V      W   */
    0x826f,0x8270,0x8271,0x8272,0x8273,0x8274,0x8275,0x8276,
/* 58  X      Y      Z      [      \      ]      ^      _   */
    0x8277,0x8278,0x8279,0x816d,0x815f,0x816e,0x814f,0x8151,
/* 60  `      a      b      c      d      e      f      g   */
    0x814d,0x8281,0x8282,0x8283,0x8284,0x8285,0x8286,0x8287,
/* 68  h      i      j      k      l      m      n      o   */
    0x8288,0x8289,0x828a,0x828b,0x828c,0x828d,0x828e,0x828f,
/* 70  p      q      r      s      t      u      v      w   */
    0x8290,0x8291,0x8292,0x8293,0x8294,0x8295,0x8296,0x8297,
/* 78  x      y      z      {      |      }      ~   */
    0x8298,0x8299,0x829a,0x816f,0x8162,0x8170,0x8160,
};



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
    printf("%s line %i: ", lexsrcnam, yylineno);
    
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
int processstring(uint8_t *dest, uint8_t *src, int doublemode)
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
        else if (c == '$' && src[si] >= '0' && src[si] <= '9')
        { /* ignore doublemode for $x codes */
            dest[di++] = '$';
            dest[di++] = src[si++];
        }
        else if (c == '%')
        { /* ignore doublemode for c-style format strings */
            if (src[si] == '%') /* ...unless it's the "literal percent sign" code */
            {
                if (doublemode)
                {
                    dest[di++] = ascjistbl['%'-0x20] >> 8;
                    dest[di++] = ascjistbl['%'-0x20] & 0xff;
                }
                else
                {
                    dest[di++] = '%';
                    dest[di++] = '%';
                }
                si++;
            }
            else
            {
                dest[di++] = '%';
                uint8_t c;
                while ((c = src[si]))
                {
                    si++;
                    dest[di++] = c;
                    if (strchr("%csdioxXufFeEaAgGnp", c)) break;
                }
            }
        }
        else if ((c < 0x80) || (c >= 0xa1 && c < 0xe0))
        {
            if (doublemode && c >= 0x20 && c < 0x7f)
            {
                dest[di++] = ascjistbl[c-0x20] >> 8;
                dest[di++] = ascjistbl[c-0x20] & 0xff;
            }
            else
            {
                dest[di++] = c;
            }
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

