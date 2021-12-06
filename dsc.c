#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include "compress.h"

#include "script.h"



int main(int argc, char *argv[])
{
    FILE *f = fopen(ROMNAME, "rb");
    if (!f)
    {
        printf("Could not open ROM file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    rewind(f);
    uint8_t *inbuf = malloc(fsize);
    fread(inbuf, 1, fsize, f);
    fclose(f);
    
    mkdir("DSC-orig");
    chdir("DSC-orig");
    
    uint8_t *inbufend = inbuf+fsize;
    
    for (uint8_t *p = inbuf; p < inbufend-0x20; p += 4)
    {
        char fnambuf[0x20];
        if (!memcmp(p, "DSC", 4))
        {
            int32_t dscindex = p-inbuf;
            uint32_t subscripts = get32(p+0x0c);
            
            uint8_t *mcmptr = inbuf+get32(p+0x08)-MAPBASE;
            
            uint32_t labeltbl[256];
            int labels = 0;
            
            uint32_t datasize = get32(mcmptr+0x04);
            uint8_t *scriptbuf = malloc(getmcmbufsize(mcmptr));
            int status = mcmuncomp(scriptbuf, inbuf, mcmptr);
            if (status)
            {
                printf("Failed to decompress MCM data at $%X with errorcode %i\n", dscindex, status);
            }
            else
            {
                /* PASS 1 - identify all labels */
                uint32_t cursuboffs = -1;
                int i = 0;
                while (i < datasize)
                {
                    /* check for subscript start */
                    for (int j = 0; j < subscripts; j++)
                    {
                        uint32_t suboffs = get32(p+0x10+(j*4));
                        if (i == suboffs)
                        {
                            cursuboffs = suboffs;
                        }
                    }
                    /* check params for any jumps */
                    uint16_t opcode = get16(scriptbuf+i);
                    i += 2;
                    if (opcode < 0x23)
                    {
                        const uint8_t *arglist = cmdtbl[opcode].arglist;
                        for (int j = 0; j < ARGLISTMAX; j++)
                        {
                            uint8_t type = arglist[j];
                            if (!type) break;
                            switch (type)
                            {
                                case A_STR:
                                    i += 2 + get16(scriptbuf+i);
                                    break;
                                case A_JUMPOFFS:
                                {
                                    uint16_t offs = get16(scriptbuf+i);
                                    uint32_t finaloffs = cursuboffs + offs*2;
                                    int foundlabel = 0;
                                    while (foundlabel < labels)
                                    {
                                        if (labeltbl[foundlabel] == finaloffs) break;
                                        foundlabel++;
                                    }
                                    if (foundlabel == labels)
                                    {
                                        if (labels == 256)
                                        {
                                            printf("$%X has too many labels\n", dscindex);
                                            goto fail;
                                        }
                                        labeltbl[labels++] = finaloffs;
                                    }
                                    /* replace jump distance with label id */
                                    write16(scriptbuf+i, foundlabel);
                                    i += 2;
                                    break;
                                }
                                case A_JUMPTABLE:
                                {
                                    uint16_t len = get16(scriptbuf+i);
                                    i += 2;
                                    while (len)
                                    {
                                        uint16_t offs = get16(scriptbuf+i);
                                        uint32_t finaloffs = cursuboffs + offs*2;
                                        int foundlabel = 0;
                                        while (foundlabel < labels)
                                        {
                                            if (labeltbl[foundlabel] == finaloffs) break;
                                            foundlabel++;
                                        }
                                        if (foundlabel == labels)
                                        {
                                            if (labels == 256)
                                            {
                                                printf("$%X has too many labels\n", dscindex);
                                                goto fail;
                                            }
                                            labeltbl[labels++] = finaloffs;
                                        }
                                        /* replace jump distance with label id */
                                        write16(scriptbuf+i, foundlabel);
                                        i += 2;
                                        len--;
                                    }
                                    break;
                                }
                                default:
                                    i += 2;
                                    break;
                            }
                        }
                    }
                }
                
                
                /* PASS 2 - output text to file */
                sprintf(fnambuf, "%06X.txt", dscindex);
                f = fopen(fnambuf, "wb");
                
                fprintf(f, "DSCBase $%06X\n\n", dscindex);
                
                uint16_t prvop = -1;
                int strnum = 0;
                i = 0;
                cursuboffs = -1;
                while (i < datasize)
                {
                    /* check for subscript start */
                    for (int j = 0; j < subscripts; j++)
                    {
                        uint32_t suboffs = get32(p+0x10+(j*4));
                        if (i == suboffs)
                        {
                            fprintf(f, "\n\n\nSubscript %i\n", j);
                            cursuboffs = suboffs;
                            prvop = -1; /* reset double-end detection */
                        }
                    }
                    /* check for label start */
                    for (int j = 0; j < labels; j++)
                    {
                        uint32_t loffs = labeltbl[j];
                        if (i == loffs)
                        {
                            fprintf(f, "Label %i\n", j);
                        }
                    }
                    
                    /* get opcode */
                    uint16_t opcode = get16(scriptbuf+i);
                    i += 2;
                    if (opcode >= 0x23)
                    {
                        fprintf(f, "NoOp%X\n", opcode);
                    } /* don't output multiple EndScripts in a row */
                    else if (opcode || prvop)
                    {
                        /* see if there is any text which must be commented */
                        int argstart = i;
                        const uint8_t *arglist = cmdtbl[opcode].arglist;
                        for (int j = 0; j < ARGLISTMAX; j++)
                        {
                            uint8_t type = arglist[j];
                            if (!type) break;
                            switch (type)
                            {
                                case A_STR:
                                {
                                    uint16_t strlen = get16(scriptbuf+i);
                                    i += 2;
                                    int stri = i;
                                    fprintf(f, "# ");
                                    while (1)
                                    {
                                        uint8_t c = scriptbuf[i++];
                                        if (!c)
                                        { /* null termination */
                                            break;
                                        }
                                        else if (c == 0x0a)
                                        { /* treat newlines separately */
                                            fprintf(f, "\n# ");
                                        }
                                        else if (c < 0x20 || c == 0x7f)
                                        { /* control code */
                                            fprintf(f, "\\x%02x", c);
                                        }
                                        else if ((c >= 0x20 && c < 0x7f) || (c >= 0xa1 && c < 0xe0))
                                        { /* printable char */
                                            fputc(c, f);
                                        }
                                        else
                                        { /* JIS character */
                                            fputc(c, f);
                                            fputc(scriptbuf[i++], f);
                                        }
                                    }
                                    i = stri + strlen;
                                    fputc('\n', f);
                                    break;
                                }
                                default:
                                    i += 2;
                                    break;
                            }
                        }
                        
                        /* now output the command and args */
                        fprintf(f, cmdtbl[opcode].name);
                        i = argstart;
                        for (int j = 0; j < ARGLISTMAX; j++)
                        {
                            uint8_t type = arglist[j];
                            if (!type) break;
                            switch (type)
                            {
                                case A_STR:
                                    fprintf(f, " \"!!! %06X %i\"", dscindex, strnum++);
                                    i += 2 + get16(scriptbuf+i);
                                    break;
                                case A_JUMPTABLE:
                                {
                                    uint16_t len = get16(scriptbuf+i);
                                    i += 2;
                                    fprintf(f, " %i", len);
                                    while (len)
                                    {
                                        fprintf(f, " %i", (uint16_t)get16(scriptbuf+i));
                                        i += 2;
                                        len--;
                                    }
                                    break;
                                }
                                default:
                                    fprintf(f, " %i", (int16_t)get16(scriptbuf+i));
                                    i += 2;
                                    break;
                            }
                        }
                        fputc('\n', f);
                    }
                    
                    prvop = opcode;
                }
                
                fclose(f);
                
                printf("Successfully exported DSC at $%06X\n", dscindex);
fail:           continue;
            }
            free(scriptbuf);
        }
    }
}

