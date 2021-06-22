#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#include "lex.h"

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




int main(int argc, char* argv[])
{
    FILE *f = fopen("hacks/hacks.gba", "rb");
    if (!f)
    {
        printf("Could not open input file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    size_t romsize = INITIALROMSIZE;
    uint8_t *rombuf = malloc(INITIALROMSIZE);
    fread(rombuf, 1, INITIALROMSIZE, f);
    fclose(f);
    
    
    
    /* ----------- import scripts ------------- */
    DIR *dir = opendir("DSC-new");
    if (!dir)
    {
        printf("Could not open DSC directory: %s\n", strerror(errno));
    }
    else
    {
        struct dirent *de;
        while ((de = readdir(dir)))
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
            lexinit(f, fnam);
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
            while ((lextype = yylex()))
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
                            if (memcmp(rombuf+dscbase, "DSC", 4))
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
                                            int len = processstring(scriptbuf+scriptlen+2, (uint8_t*)yytext+1, foundid == 0x13);
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
            uint8_t *dscptr = rombuf + dscbase;
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
            write32(dscptr+0x08, romsize | MAPBASE);
            size_t newromsize = romsize + scriptlen + 0x18;
            rombuf = realloc(rombuf, newromsize);
            uint8_t *mcmptr = rombuf + romsize;
            memcpy(mcmptr, "MCM", 4);
            write32(mcmptr+4, scriptlen);
            write32(mcmptr+8, scriptlen);
            write32(mcmptr+0xc, 1);
            write32(mcmptr+0x10, 0);
            write32(mcmptr+0x14, (romsize+0x18)|MAPBASE);
            memcpy(mcmptr+0x18, scriptbuf, scriptlen);
            printf("Successfully imported %s into ROM\n", lexsrcnam);
            romsize = newromsize;
fail:       free(scriptbuf);
            free(labeltbl);
            free(labelreftbl);
            fclose(f);
        }
        closedir(dir);
    }
    
    
    
    /* ------------- import raw strings ---------------- */
    f = fopen("text.txt", "rb");
    if (!f)
    {
        printf("Could not open %s: %s\n", "text.txt", strerror(errno));
    }
    else
    {
        lexinit(f, "text.txt");
        
        int doublemode = 0;
        int max = -1;
        uint32_t offs = -1;
        int refs = 0;
        uint32_t *reftbl = NULL;
        
        int lextype = yylex();
        while (lextype)
        {
            if (lextype == TOK_ID)
            {
                if (!strcmp(yytext, "Double"))
                {
                    doublemode = 1;
                    lextype = yylex();
                }
                else if (!strcmp(yytext, "NoDouble"))
                {
                    doublemode = 0;
                    lextype = yylex();
                }
                else if (!strcmp(yytext, "Max"))
                {
                    lextype = yylex();
                    if (lextype != TOK_NUM)
                    {
                        err_wrongtype(TOK_NUM, lextype);
                        break;
                    }
                    else
                    {
                        max = stoi(yytext);
                    }
                    lextype = yylex();
                }
                else if (!strcmp(yytext, "Abs"))
                {
                    if (offs != -1)
                    {
                        err("Abs was already defined");
                        break;
                    }
                    if (refs)
                    {
                        err("Abs definition encountered but Ptr was already defined");
                        break;
                    }
                    lextype = yylex();
                    if (lextype != TOK_NUM)
                    {
                        err_wrongtype(TOK_NUM, lextype);
                        break;
                    }
                    else
                    {
                        offs = stoi(yytext);
                    }
                    lextype = yylex();
                }
                else if (!strcmp(yytext, "Ptr"))
                {
                    if (offs != -1)
                    {
                        err("Ptr definition encountered but Abs was already defined");
                        break;
                    }
                    while ((lextype = yylex()) == TOK_NUM)
                    {
                        reftbl = realloc(reftbl, (refs+1)*sizeof(uint32_t));
                        reftbl[refs++] = stoi(yytext);
                    }
                }
                else
                {
                    err("invalid identifier %s", yytext);
                    break;
                }
            }
            else if (lextype == TOK_STRING)
            {
                uint8_t strbuf[0x400];
                yytext[strlen(yytext)-1] = 0;
                int len = processstring(strbuf, (uint8_t*)yytext+1, doublemode);
                if (len < 0) break;
                if (refs)
                {
                    offs = get32(rombuf + reftbl[0]) - MAPBASE;
                    int fail = 0;
                    for (int i = 1; i < refs; i++)
                    {
                        uint32_t thisoffs = get32(rombuf + reftbl[i]) - MAPBASE;
                        if (thisoffs != offs)
                        {
                            err("pointers at %06X and %06X are not identical", offs, thisoffs);
                            fail++;
                        }
                    }
                    if (fail) break;
                    uint32_t newoffs = offs;
                    int max = 0;
                    while (rombuf[offs+max] != 0) max++;
                    while (rombuf[offs+max] == 0) max++;
                    if (len > max)
                    {
                        newoffs = romsize;
                        /* align string length to 4-byte boundaries */
                        while (len % 4) strbuf[len++] = 0;
                        for (int i = 0; i < refs; i++)
                        {
                            write32(rombuf+reftbl[i], newoffs + MAPBASE);
                        }
                        romsize += len;
                        rombuf = realloc(rombuf, romsize);
                    }
                    memcpy(rombuf+newoffs, strbuf, len);
                }
                else
                {
                    if (len > max)
                    {
                        strbuf[max-1] = 0;
                        len = max;
                    }
                    memcpy(rombuf+offs, strbuf, len);
                }
                
                max = -1;
                offs = -1;
                refs = 0;
                free(reftbl);
                reftbl = NULL;
                
                lextype = yylex();
            }
            else
            {
                err("expecting string or identifier, got %s", tokstr(lextype));
                break;
            }
        }
        if (max != -1)
            err("WARNING: dangling Max encoutered at end of file");
        if (offs != -1)
            err("WARNING: dangling Abs encoutered at end of file");
        if (refs)
            err("WARNING: dangling Ptr encoutered at end of file");
        free(reftbl);
    }
    
    
    f = fopen("t.gba", "wb");
    fwrite(rombuf, 1, romsize, f);
    fclose(f);
}






