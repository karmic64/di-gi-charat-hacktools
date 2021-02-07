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
                                        int len = processstring(scriptbuf+scriptlen+2, yytext+1, foundid == 0x13);
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






