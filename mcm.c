#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

#include "compress.h"


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
  
  mkdir("MCM-orig"
#ifndef _WIN32
    , S_IRWXU|S_IRWXG|S_IRWXO
#endif
  );
  chdir("MCM-orig");
  
  uint8_t *inbufend = inbuf+fsize;
  
  for (uint8_t *p = inbuf; p < inbufend-0x20; p += 4)
  {
    char fnambuf[0x20];
    if (!memcmp(p, "MCM", 4))
    {
      int32_t mcmindex = p-inbuf;
      uint32_t finaldatasize = get32(p+4);
      
      uint8_t *dbuf = malloc(finaldatasize);
      int status = mcmuncomp(dbuf, inbuf, p);
      if (!status)
      {
        sprintf(fnambuf, "%06X", mcmindex);
        FILE *f = fopen(fnambuf, "wb");
        fwrite(dbuf, 1, finaldatasize, f);
        fclose(f);
        printf("Successfully extracted $%06X\n", mcmindex);
      }
      else
      {
        printf("MCM extraction at $%X failed with errorcode %d\n", mcmindex, status);
        continue;
      }
      free(dbuf);
    }
  }
  
  
  
  
  
  
  
}

