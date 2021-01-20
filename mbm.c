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
    
    mkdir("MBM");
    chdir("MBM");
    
    uint8_t *inbufend = inbuf+fsize;
    
    for (uint8_t *p = inbuf; p < inbufend-0x20; p += 4)
    {
        char fnambuf[0x20];
        if (!memcmp(p, "MBM", 4))
        {
            int32_t mbmindex = p-inbuf;
            uint16_t palsize = get16(p+0x0a);
            int nibflag = palsize > 0x10 ? 0 : 1;
            uint16_t realwidth = get16(p+6);
            uint16_t width = realwidth / (nibflag ? 2 : 1); /* width IN BYTES */
            if (nibflag && (realwidth % 2)) width++;
            uint16_t height = get16(p+8);
            uint8_t *palptr = inbuf+get32(p+0x10)-MAPBASE;
            int32_t palindex = palptr-inbuf;
            uint8_t *mcmptr = p+0x18;
            if (palindex < 0 || palindex >= fsize)
            {
                printf("Palette pointer at $%X out of bounds\n", mbmindex);
                continue;
            }
            uint16_t bmpwidth = width;
            while (bmpwidth % 4) bmpwidth++;
            
            if (memcmp(mcmptr, "MCM", 4))
            {
                printf("No MCM data found at $%X\n", mbmindex);
                continue;
            }
            int32_t mcmindex = mcmptr-inbuf;
            uint32_t finaldatasize = get32(mcmptr+4);
            if (finaldatasize < ((width * height)))
            {
                printf("MCM data too small for bitmap at $%X\n", mbmindex);
                continue;
            }
            
            uint8_t *bmpbuf = malloc(finaldatasize);
            int status = mcmuncomp(bmpbuf, inbuf, mcmptr);
            if (!status)
            {
                if (nibflag)
                {
                    for (int i = 0; i < finaldatasize; i++)
                    {
                        bmpbuf[i] = (bmpbuf[i] >> 4) | (bmpbuf[i] << 4);
                    }
                }
                sprintf(fnambuf, "%06X.bmp", mbmindex);
                FILE *f = fopen(fnambuf, "wb");
                /* header */
                fputc('B', f);
                fputc('M', f);
                fput32(14+40+(palsize*4)+(bmpwidth*height), f);
                fput32(0, f);
                fput32(14+40+(palsize*4), f);
                /* infoheader */
                fput32(40, f);
                fput32(realwidth, f);
                fput32(height, f);
                fput16(1, f);
                fput16(nibflag ? 4 : 8, f);
                fput32(0, f);
                fput32(0, f);
                fput32(0, f);
                fput32(0, f);
                fput32(palsize, f);
                fput32(0, f);
                /* palette */
                for (int i = 0; i < palsize; i++)
                {
                    uint16_t color = get16(palptr+(i*2));
                    uint8_t red = color & 0x1f;
                    uint8_t green = (color >> 5) & 0x1f;
                    uint8_t blue = (color >> 10) & 0x1f;
                    
                    fputc(blue * (255.0/31.0), f);
                    fputc(green * (255.0/31.0), f);
                    fputc(red * (255.0/31.0), f);
                    fputc(0, f);
                }
                /* bitmap */
                int tilewidth = nibflag ? 4 : 8;
                for (int y = height-1; y >= 0; y--)
                {
                    int ytile = y / 8;
                    int yfine = y % 8;
                    for (int x = 0; x < bmpwidth; x++)
                    {
                        int xtile = x / tilewidth;
                        int xfine = x % tilewidth;
                        
                        if (x > width)
                        {
                            fputc(0, f);
                        }
                        else
                        {
                            fputc(bmpbuf[(ytile*width*8) + (xtile*8*tilewidth) + (yfine*tilewidth) + xfine], f);
                        }
                    }
                }
                
                fclose(f);
                
                printf("Successfully exported $%X\n", mbmindex);
            }
            else
            {
                printf("MCM extraction at $%X failed with errorcode %d\n", mbmindex, status);
                continue;
            }
            free(bmpbuf);
        }
    }
    
    
    
    
    
    
    
}




