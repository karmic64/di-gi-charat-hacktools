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
#define CHARSPERLINE 64
            int32_t mfmindex = p-inbuf;
            sprintf(fnambuf, "%06X.bmp", mfmindex);
            uint8_t charwidth = *(p+5);
            uint8_t charheight = *(p+6);
            uint8_t bppflag = *(p+7);
            uint16_t chars = get16(p+8);
            uint16_t charbytes = get16(p+0x0a);
            uint8_t *data = inbuf+get32(p+0x10)-MAPBASE;
            
            int bmpbpp = bppflag == 0x10 ? 4 : 1;
            int pixelsperbyte = 8 / bmpbpp;
            
            int charbytewidth = charwidth / 8;
            if (charwidth % 8) charbytewidth++;
            charbytewidth *= bmpbpp;
            
            int bmpcharwidth = charwidth / pixelsperbyte;
            if (charwidth % pixelsperbyte) bmpcharwidth++;
            bmpcharwidth *= pixelsperbyte;
            int bmpwidth = bmpcharwidth * CHARSPERLINE;
            int bmpbytewidth = bmpwidth / pixelsperbyte;
            int bmpcharbytewidth = bmpcharwidth/pixelsperbyte;
            
            int bmpheight = chars / CHARSPERLINE;
            if (chars % CHARSPERLINE) bmpheight++;
            bmpheight *= charheight;
            
            int palsize = 1 << bmpbpp;
            
            size_t bmpbufsize = bmpbytewidth * bmpheight;
            
            FILE *f = fopen(fnambuf, "wb");
            fputc('B', f);
            fputc('M', f);
            fput32(14+40+(4*palsize)+bmpbufsize, f);
            fput32(0, f);
            fput32(14+40+(4*palsize), f);
            
            fput32(40, f);
            fput32(bmpwidth, f);
            fput32(bmpheight, f);
            fput16(1, f);
            fput16(bmpbpp, f);
            fput32(0, f);
            fput32(0, f);
            fput32(0, f);
            fput32(0, f);
            fput32(palsize, f);
            fput32(0, f);
            
            for (int i = 0; i < palsize; i++)
            {
                uint8_t col = (i / (float)(palsize-1)) * 255.0;
                fputc(col, f);
                fputc(col, f);
                fputc(col, f);
                fputc(0, f);
            }
            
            for (int charcoarse = (chars/CHARSPERLINE - (chars%CHARSPERLINE ? 0 : 1))*CHARSPERLINE; charcoarse >= 0; charcoarse -= CHARSPERLINE)
            {
                for (int yfine = charheight-1; yfine >= 0; yfine--)
                {
                    for (int charfine = 0; charfine < CHARSPERLINE; charfine++)
                    {
                        int c = charcoarse + charfine;
                        for (int xbyte = 0; xbyte < bmpcharbytewidth; xbyte++)
                        {
                            if (c >= chars)
                            {
                                fputc(0, f);
                            }
                            else
                            {
                                uint8_t cb = data[2 + (charbytes*c) + (yfine*charbytewidth) + xbyte];
                                if (bmpbpp == 4)
                                {
                                    fputc((cb << 4) | (cb >> 4), f);
                                }
                                else
                                {
                                    fputc(cb, f);
                                }
                            }
                        }
                    }
                }
            }
            
            fclose(f);
            
            printf("Successfully exported $%06X\n", mfmindex);
        }
        
    }
    
    
}