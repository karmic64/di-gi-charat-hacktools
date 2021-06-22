#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include "compress.h"


#define swapnib(i) (((i) >> 4) | ((i) << 4))


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
    
    mkdir("MBM-orig");
    chdir("MBM-orig");
    
    uint8_t *inbufend = inbuf+fsize;
    
    for (uint8_t *p = inbuf; p < inbufend-0x20; p += 4)
    {
        char fnambuf[0x20];
        if (!memcmp(p, "MBM", 4))
        {
            int32_t mbmindex = p-inbuf;
            uint16_t palsize = get16(p+0x0a);
            uint8_t bpp = *(p+5); /* note: bits/pixel is equal to bytes/tile row */
            int tilemapflag = *(p+4) & 4;
            uint16_t width = get16(p+6);
            uint16_t height = get16(p+8);
            uint8_t *palptr = inbuf+get32(p+0x10)-MAPBASE;
            int32_t palindex = palptr-inbuf;
            uint8_t *tilemapptr = inbuf+get32(p+0x14)-MAPBASE;
            int32_t tilemapindex = tilemapptr-inbuf;
            uint8_t *mcmptr = p+0x18;
            if (palindex < 0 || palindex >= fsize)
            {
                printf("Palette pointer at $%X out of bounds\n", mbmindex);
                continue;
            }
            if (tilemapindex < 0 || tilemapindex >= fsize)
            {
                printf("Tilemap pointer at $%X out of bounds\n", mbmindex);
                continue;
            }
            uint16_t bmpwidth = width;
            while (bmpwidth % 4) bmpwidth++;
            
            if (memcmp(mcmptr, "MCM", 4))
            {
                printf("No MCM data found at $%X\n", mbmindex);
                continue;
            }
            uint32_t finaldatasize = get32(mcmptr+4);
            
            uint8_t *tilebuf = malloc(finaldatasize);
            int status = mcmuncomp(tilebuf, inbuf, mcmptr);
            if (!status)
            {
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
                fput32(width, f);
                fput32(height, f);
                fput16(1, f);
                fput16(8, f);
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
                for (int y = height-1; y >= 0; y--)
                {
                    for (int x = 0; x < bmpwidth; x += (bpp == 8 ? 1 : 2))
                    {
                        uint16_t t = (y/8) * (width/8) + x/8;
                        if (tilemapflag)
                        {
                            t = get16(tilemapptr + (t * 2));
                        }
                        uint16_t tn = t & 0x3ff;
                        uint16_t xflip = t & 0x400;
                        uint16_t yflip = t & 0x800;
                        uint16_t pal = t / 0x1000;
                        
                        int xfine = (x & 7) ^ (xflip ? 7 : 0);
                        int yfine = (y & 7) ^ (yflip ? 7 : 0);
                        
                        uint8_t *tilerow = tilebuf + (tn*bpp*8) + (yfine*bpp);
                        
                        if (bpp == 8)
                        {
                            fputc(tilerow[xfine], f);
                        }
                        else
                        {
                            uint8_t tb = tilerow[xfine/2];
                            if (xflip) tb = swapnib(tb);
                            fputc((tb & 0x0f) + (pal * 0x10), f);
                            fputc((tb >> 4) + (pal * 0x10), f);
                        }
                    }
                }
                
                fclose(f);
                
                printf("Successfully exported $%X (%ux%u, %s, %ubpp, %u colors)\n", mbmindex
                        , width, height
                        , tilemapflag ? "tilemapped" : "bitmapped"
                        , bpp
                        , palsize);
            }
            else
            {
                printf("MCM extraction at $%X failed with errorcode %d\n", mbmindex, status);
                continue;
            }
            free(tilebuf);
        }
    }
    
    
    
    
    
    
    
}




