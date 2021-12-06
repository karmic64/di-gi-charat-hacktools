#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include <setjmp.h>
#include <png.h>

#include "compress.h"


#define scale5to8(i) (((i) << 3) | ((i) >> 2))



int32_t mbmindex;
void pngerr(png_structp p, png_const_charp e)
{
  printf("Failed to export %06X: %s\n",mbmindex,e);
  longjmp(png_jmpbuf(p),1);
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
      mbmindex = p-inbuf;
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
        printf("Palette pointer at $%06X out of bounds\n", mbmindex);
        continue;
      }
      if (tilemapindex < 0 || tilemapindex >= fsize)
      {
        printf("Tilemap pointer at $%06X out of bounds\n", mbmindex);
        continue;
      }
      if (bpp != 4 && bpp != 8)
      {
        printf("Bad bits per pixel value %u at $%06X\n", bpp, mbmindex);
        continue;
      }
      unsigned bppcolors = 1 << bpp;
      int oversizepal = palsize > bppcolors;
      unsigned outbpp = (oversizepal || bpp == 8) ? 8 : 4;
      if (width % 8)
      {
        printf("Width of $%06X is not a multiple of 8\n", mbmindex);
        continue;
      }
      if (height % 8)
      {
        printf("Height of $%06X is not a multiple of 8\n", mbmindex);
        continue;
      }
      unsigned bmprowsize = outbpp == 4 ? (width/2) : (width);
      unsigned tilewidth = width / 8;
      unsigned tileheight = height / 8;
      
      if (memcmp(mcmptr, "MCM", 4))
      {
        printf("No MCM data found at $%06X\n", mbmindex);
        continue;
      }
      uint8_t *tilebuf = malloc(getmcmbufsize(mcmptr));
      int status = mcmuncomp(tilebuf, inbuf, mcmptr);
      
      if (!status)
      {
        png_byte *bmptilerow = malloc(bmprowsize * 8);
        png_color *palette = malloc(palsize * sizeof(*palette));
        png_byte *trns = malloc(palsize);
        
        sprintf(fnambuf, "%06X.png", mbmindex);
        FILE *f = fopen(fnambuf, "wb");
        
        png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,pngerr,pngerr);
        png_infop info_ptr = png_create_info_struct(png_ptr);
        
        if (setjmp(png_jmpbuf(png_ptr)))
        {
          
        }
        else
        {
          png_init_io(png_ptr,f);
          
          png_set_IHDR(png_ptr,info_ptr,
              width, height, outbpp, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
              PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
          
          for (unsigned i = 0; i < palsize; i++)
          {
            uint16_t col = get16(palptr+i*2);
            uint8_t red = col & 0x1f;
            uint8_t green = (col >> 5) & 0x1f;
            uint8_t blue = (col >> 10) & 0x1f;
            palette[i].red = scale5to8(red);
            palette[i].green = scale5to8(green);
            palette[i].blue = scale5to8(blue);
          }
          png_set_PLTE(png_ptr,info_ptr,palette,palsize);
          
          for (unsigned i = 0; i < palsize; i++)
          {
            trns[i] = (i%bppcolors == 0) ? 0 : 0xff;
          }
          png_set_tRNS(png_ptr,info_ptr,trns,palsize,NULL);
          
          png_write_info(png_ptr,info_ptr);
          
          png_byte *rows[8];
          for (unsigned i = 0; i < 8; i++) rows[i] = &bmptilerow[bmprowsize*i];
          
          for (unsigned ty = 0; ty < tileheight; ty++)
          {
            memset(bmptilerow,0,bmprowsize*8);
            for (unsigned tx = 0; tx < tilewidth; tx++)
            {
              uint16_t tm;
              unsigned tmindex = (ty * tilewidth) + tx;
              if (tilemapflag) tm = get16(tilemapptr + (tmindex)*2);
              else tm = tmindex;
              uint16_t t = tm & 0x3ff;
              uint16_t xflip = tm & 0x400;
              uint16_t yflip = tm & 0x800;
              uint16_t pal = (tm >> 12);
              
              uint8_t *tile = tilebuf + ((8 * bpp) * t);
              for (unsigned tyf = 0; tyf < 8; tyf++)
              {
                for (unsigned txf = 0; txf < 8; txf++)
                {
                  unsigned ixf = xflip ? txf ^ 7 : txf;
                  unsigned iyf = yflip ? tyf ^ 7 : tyf;
                  
                  uint8_t col;
                  if (bpp == 4)
                  {
                    uint8_t cb = tile[(iyf*4)+(ixf/2)];
                    col = ((ixf % 2) ? (cb >> 4) : (cb & 0xf)) | (pal << 4);
                  }
                  else
                  {
                    col = tile[(iyf*8)+ixf];
                  }
                  
                  if (outbpp == 4)
                  {
                    png_byte *p = &bmptilerow[bmprowsize*tyf + ((tx*8) + txf)/2];
                    if (txf % 2) *p |= col;
                    else *p |= (col << 4);
                  }
                  else
                  {
                    bmptilerow[bmprowsize*tyf + (tx*8) + txf] = col;
                  }
                }
              }
            }
            png_write_rows(png_ptr,rows,8);
          }
          
          png_write_end(png_ptr,info_ptr);
          
          printf("Successfully exported $%06X (%ux%u, %s, %ubpp, %u colors)\n", mbmindex
            , width, height
            , tilemapflag ? "tilemapped" : "bitmapped"
            , bpp
            , palsize);
            
        }
        
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(f);
        
        free(palette);
        free(trns);
        free(bmptilerow);
      }
      else
      {
        printf("MCM extraction at $%06X failed with errorcode %d\n", mbmindex, status);
      }
      free(tilebuf);
    }
  }
  
  
  
  
  
  
  
}




