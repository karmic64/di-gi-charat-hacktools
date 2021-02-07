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
    
    mkdir("MRM");
    chdir("MRM");
    
    uint8_t *inbufend = inbuf+fsize;
    
    for (uint8_t *p = inbuf; p < inbufend-0x20; p += 4)
    {
        char fnambuf[0x20];
        if (!memcmp(p, "MRM", 4))
        {
            int32_t mrmindex = p-inbuf;
            sprintf(fnambuf, "%06X.wav", mrmindex);
            
            uint32_t datasize = get32(p+4);
            
            FILE *f = fopen(fnambuf, "wb");
            
            fwrite("RIFF", 1, 4, f);
            fput32(36 + datasize, f);
            fwrite("WAVE", 1, 4, f);
            
            fwrite("fmt ", 1, 4, f);
            fput32(16, f);
            fput16(1, f);
            fput16(1, f);
            fput32(22050, f);
            fput32(22050, f);
            fput16(1, f);
            fput16(8, f);
            
            fwrite("data", 1, 4, f);
            fput32(datasize, f);
            
            for (int i = 0; i < datasize; i++)
            {
                fputc(p[8+i] + 0x80, f);
            }
            
            fclose(f);
        }
    }
    
}