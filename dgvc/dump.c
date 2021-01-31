#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "../compress.h"
#include "../script.h"

int main(int argc, char* argv[])
{
    FILE *f = fopen("../DiGi Charat - DigiCommunication (J) [!].gba", "rb");
    if (!f)
    {
        printf("Could not open input file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    rewind(f);
    uint8_t *inbuf = malloc(fsize);
    fread(inbuf, 1, fsize, f);
    fclose(f);
    uint8_t *inbufend = inbuf+fsize;
    
    f = fopen("dump.txt", "wb");
    
    for (uint8_t *p = inbuf; p < inbufend-0x20; p += 4)
    {
        if (!memcmp(p, "DSC", 4))
        {
            int32_t dscindex = p-inbuf;
            
            uint8_t *mcmptr = inbuf+get32(p+0x08)-MAPBASE;
            
            uint32_t datasize = get32(mcmptr+0x04);
            uint8_t *scriptbuf = malloc(datasize);
            int status = mcmuncomp(scriptbuf, inbuf, mcmptr);
            if (status)
            {
                printf("Failed to decompress MCM data at $%X with errorcode %i\n", dscindex, status);
            }
            else
            {
                fprintf(f, "NEWFILE %06X.txt\n", dscindex);
                int i = 0;
                while (i < datasize)
                {
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
                                {
                                    int len = get16(scriptbuf+i);
                                    i += 2;
                                    int stri = i;
                                    if (opcode == 0x13)
                                    {
                                        fprintf(f, "\J");
                                    }
                                    while (1)
                                    {
                                        uint8_t c = scriptbuf[i++];
                                        if (!c)
                                        { /* null termination */
                                            break;
                                        }
                                        else if (c == 0x0a)
                                        { /* treat newlines separately */
                                            /* ... */
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
                                    fputc('\n', f);
                                    i = stri+len;
                                    break;
                                }
                                case A_JUMPTABLE:
                                    i += 2 + get16(scriptbuf+i)*2;
                                    break;
                                default:
                                    i += 2;
                                    break;
                            }
                        }
                    }
                }
                
                printf("Successfully exported DSC at $%06X\n", dscindex);
            }
            free(scriptbuf);
        }
    }
    fclose(f);
    
}