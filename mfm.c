#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include "compress.h"


int fput32(uint32_t i, FILE *f)
{
    for (int r = 0; r < 32; r += 8)
    {
        int s = fputc((i >> r) & 0xff, f);
        if (s == EOF) return EOF;
    }
    return 0;
}

int fput24(uint32_t i, FILE *f)
{
    for (int r = 0; r < 24; r += 8)
    {
        int s = fputc((i >> r) & 0xff, f);
        if (s == EOF) return EOF;
    }
    return 0;
}

int fput16(uint16_t i, FILE *f)
{
    for (int r = 0; r < 16; r += 8)
    {
        int s = fputc((i >> r) & 0xff, f);
        if (s == EOF) return EOF;
    }
    return 0;
}




int main(int argc, char *argv[])
{
    FILE *f = fopen("DiGi Charat - DigiCommunication (J) [!].gba", "rb");
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
    
    mkdir("MFM");
    chdir("MFM");
    
    uint8_t *inbufend = inbuf+fsize;
    
    for (uint8_t *p = inbuf; p < inbufend-0x20; p += 4)
    {
        char fnambuf[0x20];
        if (!memcmp(p, "MFM", 4))
        {
            /* note: for sanity's sake we put 64 tiles on each bitmap line */
            /* the following value must be a multiple of 4 and power of 2 */
#define BYTESPERLINE 64
            int32_t mfmindex = p-inbuf;
            sprintf(fnambuf, "%06X.bmp", mfmindex);
            uint8_t charheight = *(p+6);
            uint16_t chars = get16(p+8);
            uint16_t charbytes = get16(p+0x0a);
            uint8_t *data = inbuf+get32(p+0x10)-MAPBASE;
            
            /* if ((charbytes-2) % charheight)
            {
                printf("Could not get width of chars in $%06X\n", mfmindex);
                continue;
            } */
            
            int charwidth = (charbytes-2) / charheight;
            int charsperline = BYTESPERLINE/charwidth;
            int bmpheight = charheight*(chars/charsperline + (chars % charsperline ? 1 : 0));
            
            size_t bmpbufsize = bmpheight*BYTESPERLINE;
            
            FILE *f = fopen(fnambuf, "wb");
            fputc('B', f);
            fputc('M', f);
            fput32(14+40+(4*2)+bmpbufsize, f);
            fput32(0, f);
            fput32(14+40+(4*2), f);
            
            fput32(40, f);
            fput32(BYTESPERLINE*8, f);
            fput32(bmpheight, f);
            fput16(1, f);
            fput16(1, f);
            fput32(0, f);
            fput32(0, f);
            fput32(0, f);
            fput32(0, f);
            fput32(2, f);
            fput32(0, f);
            
            fput32(0, f); /* black and white */
            fput32(0xffffff, f);
            
            for (int charhi = ((chars/charsperline + (chars % charsperline ? 1 : 0))-1)*charsperline; charhi >= 0; charhi -= charsperline)
            {
                for (int row = charheight-1; row >= 0; row--)
                {
                    for (int charlo = 0; charlo < charsperline; charlo++)
                    {
                        int ci = charhi | charlo;
                        if (ci >= chars)
                        {
                            for (int i = 0; i < charwidth; i++)
                                fputc(0, f);
                        }
                        else
                        {
                            uint8_t *c = data + (charbytes*ci) + (row*charwidth) + 2;
                            fwrite(c, 1, charwidth, f);
                        }
                    }
                }
            }
            
            fclose(f);
            
            printf("Successfully exported $%06X\n", mfmindex);
        }
        
    }
    
    
}