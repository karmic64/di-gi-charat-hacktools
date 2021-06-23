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
    
    uint32_t injectmcm(uint32_t offs, uint8_t *data, size_t size)
    {
        if (memcmp(rombuf+offs, "MCM", 4)) return -1;
        
        /* get the size of the original packed data */
        /* so we can tell if we can just replace it or have to put it at the end */
        uint32_t csrcsize = get32(rombuf+offs+0x04);
        uint32_t cblocksize = get32(rombuf+offs+0x08);
        uint32_t cmpblocks = get32(rombuf+offs+0x0c);
        uint32_t cdatasize = 0;
        
        for (int i = 0; i < cmpblocks-1; i++)
            cdatasize += get32(rombuf+offs+0x14+((i+1)*4)) - get32(rombuf+offs+0x14+(i*4));
        
        uint8_t firstcomp = *(rombuf+offs+0x10);
        uint8_t *cdata = rombuf + get32(rombuf+offs+0x14+((cmpblocks-1)*4)) - MAPBASE;
        /* make a fake run through the compressed data */
        switch (firstcomp)
        {
            case 0:
            {
                cdatasize += csrcsize - (cblocksize*(cmpblocks-1));
                break;
            }
            case 1:
            {
                /* rle */
                int32_t uncsize = get24(cdata+1);
                size_t si = 4;
                
                while (uncsize > 0)
                {
                    uint8_t block = cdata[si++];
                    uint8_t size;
                    if (block & 0x80)
                        size = (block & 0x7f) + 3;
                    else
                        size = (block & 0x7f) + 1;
                    
                    si += (block & 0x80) ? 1 : size;
                    uncsize -= size;
                }
                
                cdatasize += si;
                break;
            }
            case 2:
            {
                /* lz77 */
                int32_t uncsize = get24(cdata+1);
                size_t bi = 4;
                size_t si = 5;
                uint8_t blockbit = 0;
                
                while (uncsize > 0)
                {
                    if (blockbit >= 8)
                    {
                        blockbit = 0;
                        bi = si;
                        si++;
                    }
                    if (cdata[bi] & (0x80 >> blockbit))
                    {
                        uncsize -= ((cdata[si] >> 4) + 3);
                        si += 2;
                    }
                    else
                    {
                        uncsize--;
                        si++;
                    }
                    blockbit++;
                }
                
                cdatasize += si;
                break;
            }
            case 3:
            {
                /* huffman */
                uint8_t bits = cdata[0] & 0x0f;
                
                int32_t uncbits = get24(cdata+1) * 8;
                size_t si = ((cdata[4]+1)*2)+4;
                size_t ti = 5;
                
                uint8_t sb = 0;
                
                while (uncbits > 0)
                {
                    uint8_t block = (cdata[si + ((sb/8) ^ 3)] & (0x80 >> (sb & 7))) ? 1 : 0;
                    
                    if (cdata[ti] & (0x80 >> block))
                    {
                        uncbits -= bits;
                        ti = 5;
                    }
                    else
                    {
                        ti = (ti & ~1) + (cdata[ti] & 0x3f)*2 + 2 + block;
                    }
                    
                    if (++sb == 32)
                    {
                        sb = 0;
                        si += 4;
                    }
                }
                
                cdatasize += si + (sb ? 4 : 0);
                break;
            }
            default:
            {
                cdatasize += get24(cdata+1);
                break;
            }
        }
        
        
        /* now finally pack the data */
        uint8_t *lzbuf = malloc(size+0x80);
        uint8_t *huffbuf = malloc(size+0x80);
        
        int lzsize = lz77comp(lzbuf, data, size);
        int huffsize;
        if (lzsize >= 0)
            huffsize = huffcomp(huffbuf, lzbuf, lzsize);
        else
            huffsize = huffcomp(huffbuf, data, size);
        
        uint32_t finalsize;
        uint8_t *finaldata;
        if (huffsize >= 0)
        {
            finalsize = huffsize;
            finaldata = huffbuf;
        }
        else if (lzsize >= 0)
        {
            finalsize = lzsize;
            finaldata = lzbuf;
        }
        else
        {
            finalsize = size;
            finaldata = data;
        }
        
        uint32_t ui;
        while (finalsize & 3) finalsize++;
        if (finalsize > cdatasize)
        { /* too big, we need to add the data to the end of the rom */
            ui = romsize;
            romsize += finalsize;
            rombuf = realloc(rombuf, romsize);
        }
        else
        {
            ui = offs+0x18;
        }
        
        memcpy(rombuf+ui, finaldata, finalsize);
        free(lzbuf);
        free(huffbuf);
        
        uint32_t bufsize = 0x200;
        while (bufsize < size) bufsize *= 2;
        
        write32(rombuf+offs+4, size);
        write32(rombuf+offs+8, bufsize);
        write32(rombuf+offs+0x0c, 1);
        write32(rombuf+offs+0x14, ui+MAPBASE);
        memset(rombuf+offs+0x10, 0, 4);
        if (huffsize >= 0 && lzsize >= 0)
        {
            rombuf[offs+0x10] = 3;
            rombuf[offs+0x11] = 2;
        }
        else if (!(huffsize >= 0) && (lzsize >= 0))
        {
            rombuf[offs+0x10] = 2;
        }
        else if ((huffsize >= 0) && !(lzsize >= 0))
        {
            rombuf[offs+0x10] = 3;
        }
        
        return finalsize;
    }
    
    char *initialcwd = getcwd(NULL, 0);
    
    /* ----------- import scripts ------------- */
    puts("Compiling scripts...");
    int status = chdir("DSC-new");
    if (status)
    {
        printf("Could not change to DSC directory: %s\n", strerror(errno));
    }
    else
    {
        DIR *dir = opendir(".");
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
                            goto dsc_fail;
                        case TOK_ID:
                            if (!strcmp(yytext, "DSCBase"))
                            {
                                lextype = yylex();
                                if (lextype != TOK_NUM)
                                {
                                    err_wrongtype(TOK_NUM, lextype);
                                    goto dsc_fail;
                                }
                                if (dscbase != -1)
                                {
                                    err("DSCBase was already defined at line %i", dscline);
                                    goto dsc_fail;
                                }
                                dscbase = stoi(yytext);
                                dscline = yylineno;
                                if (dscbase >= INITIALROMSIZE)
                                {
                                    err("DSCBase value out of bounds");
                                    goto dsc_fail;
                                }
                                if (memcmp(rombuf+dscbase, "DSC", 4))
                                {
                                    err("DSCBase does not point to valid DSC data");
                                    goto dsc_fail;
                                }
                            }
                            else if (!strcmp(yytext, "Subscript"))
                            {
                                lextype = yylex();
                                if (lextype != TOK_NUM)
                                {
                                    err_wrongtype(TOK_NUM, lextype);
                                    goto dsc_fail;
                                }
                                int subid = stoi(yytext);
                                if (subid >= MAX_SUBSCRIPTS)
                                {
                                    err("subscript number too high");
                                    goto dsc_fail;
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
                                    goto dsc_fail;
                                }
                                int labelid = stoi(yytext);
                                int foundlab = 0;
                                while (foundlab < labels)
                                {
                                    if (labeltbl[foundlab].id == labelid)
                                    {
                                        err("label %i was already defined at line %i", labelid, labeltbl[foundlab].line);
                                        goto dsc_fail;
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
                                    goto dsc_fail;
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
                                                    goto dsc_fail;
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
                                                    goto dsc_fail;
                                                }
                                                yytext[strlen(yytext)-1] = '\0'; /* remove quotes */
                                                int len = processstring(scriptbuf+scriptlen+2, (uint8_t*)yytext+1, foundid == 0x13);
                                                if (len < 0) goto dsc_fail;
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
                                                    goto dsc_fail;
                                                }
                                                if (cursubscript == -1)
                                                {
                                                    err("labels may not be referenced outside of subscript");
                                                    goto dsc_fail;
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
                                                    goto dsc_fail;
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
                                                        goto dsc_fail;
                                                    }
                                                    if (cursubscript == -1)
                                                    {
                                                        err("labels may not be referenced before defining subscript");
                                                        goto dsc_fail;
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
                            goto dsc_fail;
                    }
                }
                if (dscbase == -1)
                {
                    err("end of file encountered but no DSCBase defined");
                    goto dsc_fail;
                }
                if (!subscripts)
                {
                    err("end of file encountered but no subscripts defined");
                    goto dsc_fail;
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
                        goto dsc_fail;
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
                    goto dsc_fail;
                }
                for (int i = 0; i < subscripts; i++)
                {
                    write32(dscptr+0x10+(i*4), subscripttbl[i]);
                }
                write32(dscptr+0x10+(subscripts*4), scriptlen);
                int status = injectmcm(get32(dscptr+0x08)-MAPBASE, scriptbuf, scriptlen);
                if (status >= 0) 
                    printf("Successfully imported %s into ROM\n", lexsrcnam);
                else
                    printf("Couldn't inject MCM for %s\n", lexsrcnam);
dsc_fail:       free(scriptbuf);
                free(labeltbl);
                free(labelreftbl);
                fclose(f);
            }
            closedir(dir);
        }
    }
    chdir(initialcwd);
    
    
    
    /* ------------- import raw strings ---------------- */
    puts("\nImporting raw text...");
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
        
        fclose(f);
        puts("OK.");
    }
    
    
    
    
    /* ----------------- import bitmaps ---------------- */
    puts("\nImporting graphics...");
    status = chdir("MBM-new");
    if (status)
    {
        printf("Couldn't change to MBM directory: %s\n", strerror(errno));
    }
    else
    {
        DIR *dir = opendir(".");
        if (!dir)
        {
            printf("Couldn't open MBM directory: %s\n", strerror(errno));
        }
        else
        {
            struct palette
            {
                uint32_t offs;
                uint16_t size; /* in colors, not bytes */
            };
            
            unsigned palettes = 0;
            unsigned palmax = 0x100;
            
            struct palette *paltbl = malloc(palmax * sizeof(*paltbl));
            
            void addpalent(uint32_t offs, uint16_t size)
            {
                if (palettes == palmax)
                {
                    palmax *= 2;
                    paltbl = realloc(paltbl, palmax * sizeof(*paltbl));
                }
                paltbl[palettes].offs = offs;
                paltbl[palettes].size = size;
                palettes++;
            }
            
            uint32_t addpal(uint8_t *pal, uint16_t size)
            {
                for (unsigned i = 0; i < palettes; i++)
                {
                    uint32_t o = paltbl[i].offs;
                    uint16_t s = paltbl[i].size;
                    if (size > s) continue;
                    unsigned j = 0;
                    for ( ; j < size; j++)
                    {
                        if (pal[j*2] != rombuf[o + j*2] ||
                            (pal[j*2 + 1]&0x7f) != (rombuf[o + j*2 + 1]&0x7f)) break;
                    }
                    if (j == size) return o;
                }
                
                uint32_t s = size*2;
                if (s & 3) s += 2;
                
                uint32_t pi = romsize;
                romsize += s;
                rombuf = realloc(rombuf, romsize);
                
                memcpy(rombuf+pi, pal, s);
                
                addpalent(pi, size);
                return pi;
            }
            
            /* index all used palettes */
            for (uint32_t o = 0; o < INITIALROMSIZE-0x20; o+=4)
            {
                if (!memcmp(rombuf+o, "MBM", 4))
                {
                    uint32_t offs = get32(rombuf+o+0x10) - MAPBASE;
                    uint16_t size = get16(rombuf+o+0x0a);
                    
                    int found = 0;
                    for (unsigned i = 0; i < palettes; i++)
                    {
                        if (offs == paltbl[i].offs)
                        {
                            found++;
                            if (size > paltbl[i].size) paltbl[i].size = size;
                            break;
                        }
                    }
                    if (!found) addpalent(offs,size);
                }
            }
            
            
            struct dirent *de = NULL;
            while ((de = readdir(dir)))
            {
                char *fnam = de->d_name;
                
                if (!strcmp(fnam, ".")) continue;
                if (!strcmp(fnam, "..")) continue;
                
                uint32_t offs;
                int status = sscanf(fnam, "%06X.bmp", &offs);
                if (!status) continue;
                
                lexsrcnam = fnam;
                yylineno = -1;
                
                if (memcmp(rombuf+offs, "MBM", 4))
                {
                    err("no MBM found at $%06X", offs);
                    continue;
                }
                
                FILE *f = fopen(fnam, "rb");
                if (!f)
                {
                    printf("Couldn't open %s: %s\n", fnam, strerror(errno));
                    continue;
                }
                
                uint8_t bmpheader[0x36];
                uint8_t *palette = NULL;
                uint8_t *tilebuf = NULL;
                uint8_t *tilemap = NULL;
                fread(bmpheader, 1, 0x36, f);
                if (ferror(f))
                {
                    err("error reading header: %s", strerror(errno));
                    goto mbm_fail;
                }
                if (feof(f))
                {
                    err("unexpected end-of-file while reading header");
                    goto mbm_fail;
                }
                
                if (bmpheader[0] != 'B' || bmpheader[1] != 'M')
                {
                    err("not a BMP");
                    goto mbm_fail;
                }
                int errs = 0;
                uint32_t bmpoffs = get32(bmpheader+0x0a);
                uint32_t bmpheadersize = get32(bmpheader+0x0e);
                uint32_t bmpwidth = get32(bmpheader+0x12);
                uint32_t bmpheight = get32(bmpheader+0x16);
                uint16_t bmpbpp = get16(bmpheader+0x1c);
                uint32_t bmpcolors = get32(bmpheader+0x2e);
                
                if (bmpoffs < 0x36+(bmpcolors*4))
                {
                    err("bad bitmap offset");
                    errs++;
                }
                uint32_t realwidth = get16(rombuf+offs+6);
                uint32_t realheight = get16(rombuf+offs+8);
                if (bmpwidth != realwidth || bmpheight != realheight)
                {
                    err("bad resolution (should be %ux%u, is %ux%u)",realwidth,realheight,bmpwidth,bmpheight);
                    errs++;
                }
                if (bmpbpp != 8 && bmpbpp != 4)
                {
                    err("only 4bpp and 8bpp images are supported");
                    errs++;
                }
                uint8_t realbpp = *(rombuf+offs+4) & 1 ? 8 : 4;
                uint8_t tilesize = realbpp == 8 ? 64 : 32;
                if (get32(bmpheader+0x1e))
                {
                    err("compressed images are not supported");
                    errs++;
                }
                uint16_t realcolors = get16(rombuf+offs+0x0a);
                if (realcolors < bmpcolors)
                {
                    err("bad palette size (should be %u, is %u)", realcolors, bmpcolors);
                    errs++;
                }
                
                if (errs) goto mbm_fail;
                
                
                palette = malloc(bmpcolors*2);
                fseek(f, bmpheadersize+0x0e, SEEK_SET);
                for (unsigned i = 0; i < bmpcolors; i++)
                {
                    uint8_t blue = fgetc(f) >> 3;
                    uint8_t green = fgetc(f) >> 3;
                    uint8_t red = fgetc(f) >> 3;
                    fgetc(f);
                    
                    if (ferror(f))
                    {
                        err("error reading palette: %s", strerror(errno));
                        goto mbm_fail;
                    }
                    if (feof(f))
                    {
                        err("unexpected end-of-file while reading palette");
                        goto mbm_fail;
                    }
                    
                    uint16_t color = red | (green << 5) | (blue << 10);
                    write16(palette+i*2, color);
                }
                
                uint8_t tilemapflag = *(rombuf+offs+4) & 4;
                uint16_t tilenum = 0;
                uint16_t maxtiles = (bmpwidth*bmpheight)/64;
                tilebuf = malloc(maxtiles * tilesize);
                unsigned tmi = 0;
                
                if (tilemapflag) tilemap = malloc(maxtiles*2);
                
                unsigned bmprowsize = bmpwidth / ((bmpbpp == 8) ? 1 : 2);
                while (bmprowsize & 3) bmprowsize++;
                
                for (unsigned ytile = 0; ytile < bmpheight/8; ytile++)
                {
                    for (unsigned xtile = 0; xtile < bmpwidth/8; xtile++)
                    {
                        uint8_t *tile = tilebuf + tilenum*tilesize;
                        for (unsigned yfine = 0; yfine < 8; yfine++)
                        {
                            fseek(f, bmpoffs + (bmpheight-1-(ytile*8+yfine))*bmprowsize + (xtile*8*8/bmpbpp), SEEK_SET);
                            
                            uint8_t row[8];
                            
                            if (bmpbpp == 8)
                            {
                                fread(row, 1, 8, f);
                            }
                            else
                            {
                                for (unsigned x = 0; x < 8; x+=2)
                                {
                                    uint8_t v = fgetc(f);
                                    row[x] = v >> 4;
                                    row[x+1] = v & 0x0f;
                                }
                            }
                            
                            uint8_t *tr = tile + (yfine*realbpp);
                            if (realbpp == 8)
                            {
                                memcpy(tr, row, 8);
                            }
                            else
                            {
                                for (unsigned x = 0; x < 8; x+=2)
                                {
                                    tr[x/2] = row[x] | row[x+1] << 4;
                                }
                            }
                        }
                        if (tilemapflag)
                        {
                            unsigned i = 0;
                            for (  ; i < tilenum; i++)
                            {
                                if (!memcmp(tile, tilebuf + i*tilesize, tilesize)) break;
                            }
                            write16(tilemap + (tmi++)*2, i);
                            if (i == tilenum) tilenum++;
                        }
                        else
                        {
                            tilenum++;
                        }
                    }
                }
                
                
                
                uint32_t paloffs = addpal(palette, bmpcolors);
                write32(rombuf+offs+0x10, paloffs + MAPBASE);
                if (tilemapflag)
                {
                    memcpy(rombuf+get32(rombuf+offs+0x14)-MAPBASE, tilemap, maxtiles*2);
                }
                write16(rombuf+offs+0x0e, tilenum);
                
                injectmcm(offs+0x18, tilebuf, tilenum * tilesize);
                
                printf("Successfully imported %s into ROM\n", fnam);
                
mbm_fail:       fclose(f);
                free(palette);
                free(tilebuf);
                free(tilemap);
                
            }
            
            
            
            
            
            free(paltbl);
            closedir(dir);
        }
    }
    chdir(initialcwd);
    
    
    
    
    f = fopen("t.gba", "wb");
    fwrite(rombuf, 1, romsize, f);
    fclose(f);
}






