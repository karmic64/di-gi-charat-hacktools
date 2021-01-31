#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>
#include <dirent.h>

#include "lex.yy.c"

#include "compress.h"

#include "script.h"

#define MAX_SUBSCRIPTS 256

typedef struct
{
    int id;
    int line; /* line of definition */
    uint32_t offs; /* from start of script */
} label_t;

typedef struct
{
    int label;
    int line; /* to report errors */
    uint8_t *ptr; /* to the final data */
    int subscript;
} labelref_t;

/* data is appended starting from here */
#define INITIALROMSIZE 0x7f2000


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
    while (c = tolower(s[si++]))
    {
        uint8_t d;
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
    int doublemode = 0;
    uint8_t c;
    while (c = src[si++])
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
                case 'J':
                    doublemode++;
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



int main(int argc, char* argv[])
{
    FILE *f = fopen("hacks/hacks.gba", "rb");
    if (!f)
    {
        printf("Could not open input file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    size_t fsize = INITIALROMSIZE;
    uint8_t *inbuf = malloc(INITIALROMSIZE);
    fread(inbuf, 1, INITIALROMSIZE, f);
    fclose(f);
    
    
    
    /* ----------- import scripts ------------- */
    char* initialcwd = getcwd(NULL, 0);
    int status = chdir("DSC");
    if (status)
    {
        printf("Could not change to DSC directory: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    
    DIR *dir = opendir(".");
    if (!dir)
    {
        printf("Could not open DSC directory: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    struct dirent *de;
    while (de = readdir(dir))
    {
        char *fnam = de->d_name;
        if (!strcmp(fnam, ".")) continue;
        if (!strcmp(fnam, "..")) continue;
        FILE *f = fopen(fnam, "rb");
        if (!f)
        {
            printf("Could not open %s: %s\n", fnam, strerror(errno));
            continue;
        }
        yyrestart(f);
        yylineno = 1;
        lexsrcnam = fnam;
        uint32_t subscripttbl[MAX_SUBSCRIPTS];
        memset(subscripttbl, 0, MAX_SUBSCRIPTS*sizeof(uint32_t));
        int subscripts = 0;
        uint32_t cursubscript = -1;
        uint8_t *scriptbuf = malloc(0x20000);
        memset(scriptbuf, 0, 0x20000);
        int scriptlen = 0;
        uint32_t dscbase = -1;
        int dscline;
        label_t *labeltbl = NULL;
        int labels = 0;
        labelref_t *labelreftbl = NULL;
        int labelrefs = 0;
        int lextype;
        while (lextype = yylex())
        {
            switch (lextype)
            {
                case TOK_INVALID:
                    err("parse error");
                    goto fail;
                case TOK_ID:
                    if (!strcmp(yytext, "DSCBase"))
                    {
                        lextype = yylex();
                        if (lextype != TOK_NUM)
                        {
                            err_wrongtype(TOK_NUM, lextype);
                            goto fail;
                        }
                        if (dscbase != -1)
                        {
                            err("DSCBase was already defined at line %i", dscline);
                            goto fail;
                        }
                        dscbase = stoi(yytext);
                        dscline = yylineno;
                        if (dscbase >= INITIALROMSIZE)
                        {
                            err("DSCBase value out of bounds");
                            goto fail;
                        }
                        if (memcmp(inbuf+dscbase, "DSC", 4))
                        {
                            err("DSCBase does not point to valid DSC data");
                            goto fail;
                        }
                    }
                    else if (!strcmp(yytext, "Subscript"))
                    {
                        lextype = yylex();
                        if (lextype != TOK_NUM)
                        {
                            err_wrongtype(TOK_NUM, lextype);
                            goto fail;
                        }
                        int subid = stoi(yytext);
                        if (subid >= MAX_SUBSCRIPTS)
                        {
                            err("subscript number too high");
                            goto fail;
                        }
                        cursubscript = subid;
                        /* align subscripts to 4-bytes */
                        while (scriptlen % 4) scriptlen++;
                        subscripttbl[subid] = scriptlen;
                        if (subid + 1 >= subscripts)
                        {
                            subscripts = subid+1;
                        }
                    }
                    else if (!strcmp(yytext, "Label"))
                    {
                        lextype = yylex();
                        if (lextype != TOK_NUM)
                        {
                            err_wrongtype(TOK_NUM, lextype);
                            goto fail;
                        }
                        int labelid = stoi(yytext);
                        int foundlab = 0;
                        while (foundlab < labels)
                        {
                            if (labeltbl[foundlab].id == labelid)
                            {
                                err("label %i was already defined at line %i", labelid, labeltbl[foundlab].line);
                                goto fail;
                            }
                            foundlab++;
                        }
                        labeltbl = realloc(labeltbl, (labels+1)*sizeof(label_t));
                        labeltbl[labels].id = labelid;
                        labeltbl[labels].line = yylineno;
                        labeltbl[labels].offs = scriptlen;
                        labels++;
                    }
                    else
                    {
                        int foundid = -1;
                        for (int i = 0; i < 0x23; i++)
                        {
                            if (!strcmp(yytext, cmdtbl[i].name))
                            {
                                foundid = i;
                                break;
                            }
                        }
                        if (foundid < 0)
                        {
                            err("invalid identifier %s", yytext);
                            goto fail;
                        }
                        else
                        {
                            write16(scriptbuf+scriptlen, foundid);
                            scriptlen += 2;
                            for (int j = 0; j < ARGLISTMAX; j++)
                            {
                                uint8_t argtype = cmdtbl[foundid].arglist[j];
                                if (!argtype) break;
                                lextype = yylex();
                                switch (argtype)
                                {
                                    case A_NUM:
                                        if (lextype != TOK_NUM)
                                        {
                                            err_wrongtype(TOK_NUM, lextype);
                                            goto fail;
                                        }
                                        int len = stoi(yytext);
                                        write16(scriptbuf+scriptlen, len);
                                        scriptlen += 2;
                                        break;
                                    case A_STR:
                                    {
                                        if (lextype != TOK_STRING)
                                        {
                                            err_wrongtype(TOK_STRING, lextype);
                                            goto fail;
                                        }
                                        yytext[strlen(yytext)-1] = '\0'; /* remove quotes */
                                        int len = processstring(scriptbuf+scriptlen+2, yytext+1);
                                        if (len < 0) goto fail;
                                        while (len % 2) len++; /* align end to word */
                                        write16(scriptbuf+scriptlen, len);
                                        scriptlen += len + 2;
                                        break;
                                    }
                                    case A_JUMPOFFS:
                                    {
                                        if (lextype != TOK_NUM)
                                        {
                                            err_wrongtype(TOK_NUM, lextype);
                                            goto fail;
                                        }
                                        if (cursubscript == -1)
                                        {
                                            err("labels may not be referenced outside of subscript");
                                            goto fail;
                                        }
                                        int labelid = stoi(yytext);
                                        labelreftbl = realloc(labelreftbl, (labelrefs+1)*sizeof(labelref_t));
                                        labelreftbl[labelrefs].label = labelid;
                                        labelreftbl[labelrefs].line = yylineno;
                                        labelreftbl[labelrefs].ptr = scriptbuf+scriptlen;
                                        labelreftbl[labelrefs].subscript = cursubscript;
                                        labelrefs++;
                                        scriptlen += 2;
                                        break;
                                    }
                                    case A_JUMPTABLE:
                                    {
                                        if (lextype != TOK_NUM)
                                        {
                                            err_wrongtype(TOK_NUM, lextype);
                                            goto fail;
                                        }
                                        uint16_t len = stoi(yytext);
                                        write16(scriptbuf+scriptlen, len);
                                        scriptlen += 2;
                                        while (len)
                                        {
                                            lextype = yylex();
                                            if (lextype != TOK_NUM)
                                            {
                                                err_wrongtype(TOK_NUM, lextype);
                                                goto fail;
                                            }
                                            if (cursubscript == -1)
                                            {
                                                err("labels may not be referenced before defining subscript");
                                                goto fail;
                                            }
                                            int labelid = stoi(yytext);
                                            labelreftbl = realloc(labelreftbl, (labelrefs+1)*sizeof(labelref_t));
                                            labelreftbl[labelrefs].label = labelid;
                                            labelreftbl[labelrefs].line = yylineno;
                                            labelreftbl[labelrefs].ptr = scriptbuf+scriptlen;
                                            labelreftbl[labelrefs].subscript = cursubscript;
                                            labelrefs++;
                                            scriptlen += 2;
                                            len--;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    break;
                default:
                    err_wrongtype(TOK_ID, lextype);
                    goto fail;
            }
        }
        if (dscbase == -1)
        {
            err("end of file encountered but no DSCBase defined");
            goto fail;
        }
        if (!subscripts)
        {
            err("end of file encountered but no subscripts defined");
            goto fail;
        }
        /* resolve all label references */
        for (int i = 0; i < labelrefs; i++)
        {
            int targetlabel = labelreftbl[i].label;
            yylineno = labelreftbl[i].line;
            int foundlabel = 0;
            while (foundlabel < labels)
            {
                if (targetlabel == labeltbl[foundlabel].id) break;
                foundlabel++;
            }
            if (foundlabel == labels)
            {
                err("label %i is undefined", targetlabel);
                goto fail;
            }
            write16(labelreftbl[i].ptr, (labeltbl[foundlabel].offs - subscripttbl[labelreftbl[i].subscript])/2);
        }
        /* align script end to 4-bytes */
        while (scriptlen % 4) scriptlen++;
        /* inject new script into rom */
        uint8_t *dscptr = inbuf + dscbase;
        uint32_t realsubs = get32(dscptr+0x0c);
        if (subscripts != realsubs)
        {
            err("subscripts in file is not the same as subscripts in ROM");
            goto fail;
        }
        for (int i = 0; i < subscripts; i++)
        {
            write32(dscptr+0x10+(i*4), subscripttbl[i]);
        }
        write32(dscptr+0x10+(subscripts*4), scriptlen);
        write32(dscptr+0x08, fsize | MAPBASE);
        size_t newfsize = fsize + scriptlen + 0x18;
        inbuf = realloc(inbuf, newfsize);
        uint8_t *mcmptr = inbuf + fsize;
        memcpy(mcmptr, "MCM", 4);
        write32(mcmptr+4, scriptlen);
        write32(mcmptr+8, scriptlen);
        write32(mcmptr+0xc, 1);
        write32(mcmptr+0x10, 0);
        write32(mcmptr+0x14, (fsize+0x18)|MAPBASE);
        memcpy(mcmptr+0x18, scriptbuf, scriptlen);
        printf("Successfully imported %s into ROM\n", lexsrcnam);
        fsize = newfsize;
fail:   free(scriptbuf);
        free(labeltbl);
        free(labelreftbl);
        fclose(f);
    }
    closedir(dir);
    
    
    
    
    chdir(initialcwd);
    f = fopen("t.gba", "wb");
    fwrite(inbuf, 1, fsize, f);
    fclose(f);
}






