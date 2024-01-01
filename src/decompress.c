
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "decompress.h"

#include "rom.h"




/*******************************************************/

int huffuncomp(uint8_t *dest, uint8_t *src)
{
    if (((*src) & 0xf0) != 0x20) return -1;
    uint8_t databits = (*src) & 0x0f;
    if (!databits) return -1;
    
    uint32_t datasize = get24(src+1);
    memset(dest, 0, datasize);
    
    uint8_t *sp = src+4 + (((*(src+4))+1) * 2);
    uint8_t *tp = src+5; /* current tree data node */
    uint8_t *dp = dest;
    uint8_t *de = dest+datasize;
    
    uint32_t compdata = get32(sp); /* current data read from source */
    sp += 4;
    uint8_t nodeend = 0; /* 1 if data node found */
    uint8_t sdbit = 0; /* source data bit (0 -> 31) */
    uint8_t tdbit = 0; /* tree data bit (0 -> databits) */
    uint8_t ddbit = 0; /* dest data bit (0 -> 7) */
    
    while (dp < de)
    {
        if (nodeend) /* data node found? */
        {
            /* turn on the relevant bit in the destination if needed */
            uint8_t bit = ((*tp) & (1 << (tdbit))) ? 1 : 0;
            *dp |= bit << (ddbit);
            
            if (++ddbit >= 8)
            {
                ddbit = 0;
                dp++;
            }
            if (++tdbit >= databits)
            {
                tp = src+5; /* return to the start of the tree */
                nodeend = 0;
                tdbit = 0;
            }
        }
        else /* otherwise get new source bit */
        {
            uint8_t nodeinfo = *tp;
            uint8_t node = (compdata & (1 << (31-sdbit)) ? 1 : 0);
            
            tp = (uint8_t*)((uintptr_t)tp & (~1)) + ((nodeinfo & 0x3f) * 2)+2 + node;
            nodeend = nodeinfo & (0x80 >> node);
            
            if (++sdbit >= 32)
            {
                sdbit = 0;
                compdata = get32(sp);
                sp += 4;
            }
        }
    }
    
    
    return 0;
}


/******************************************************/

int lz77uncomp(uint8_t *dest, uint8_t *src)
{
    if (*src != 0x10) return -1;
    
    uint32_t datasize = get24(src+1);
    memset(dest, 0, datasize);
    
    uint8_t blockcnt = 0;
    uint8_t blockflags = 0;
    uint8_t repcnt = 0;
    uint32_t repdisp;
    
    uint8_t *sp = src+4;
    uint8_t *dp = dest;
    uint8_t *de = dest+datasize;
    
    while (dp < de)
    {
        if (!blockcnt && !repcnt) /* new blockflags needed? */
        {
            blockflags = *(sp++);
        }
        if (repcnt) /* compressed copy in progress? */
        {
            *dp = *(dp-repdisp-1);
            dp++;
            repcnt--;
        }
        else /* otherwise get new block */
        {
            if (blockflags & (0x80 >> blockcnt)) /* compressed */
            {
                uint16_t repinfo = get16(sp);
                repcnt = ((repinfo >> 4) & 0x0f) + 3;
                repdisp = (repinfo >> 8) | ((repinfo & 0x0f) << 8);
                sp += 2;
            }
            else /* uncompressed */
            {
                *(dp++) = *(sp++);
            }
            if (++blockcnt >= 8) blockcnt = 0;
        }
    }
    
    return 0;
}


/******************************************************/

int diff8bitunfilter(uint8_t *dest, uint8_t *src)
{
    if (*src != 0x81) return -1;
    
    uint32_t datasize = get24(src+1);
    
    uint8_t lastdata = 0;
    
    uint8_t *sp = src+4;
    uint8_t *dp = dest;
    uint8_t *de = dest+datasize;
    
    while (dp < de)
    {
        lastdata += *(sp++);
        *(dp++) = lastdata;
    }
    
    
    return 0;
}


/******************************************************/

int diff16bitunfilter(uint8_t *dest, uint8_t *src)
{
    if (*src != 0x82) return -1;
    
    uint32_t datasize = get24(src+1);
    
    uint16_t lastdata = 0;
    
    uint8_t *sp = src+4;
    uint8_t *dp = dest;
    uint8_t *de = dest+datasize;
    
    while (dp < de)
    {
        lastdata += get16(sp);
        sp += 2;
        write16(dp, lastdata);
        dp += 2;
    }
    
    
    return 0;
}


/******************************************************/

int rluncomp(uint8_t *dest, uint8_t *src)
{
    if (((*src) & 0xf0) != 0x30) return -1;
    
    uint32_t datasize = get24(src+1);
    
    uint8_t repcnt = 0;
    int compdata = -1;
    
    uint8_t *sp = src+4;
    uint8_t *dp = dest;
    uint8_t *de = dest+datasize;
    
    while (dp < de)
    {
        if (!repcnt)
        {
            uint8_t cmpinfo = *(sp++);
            if (cmpinfo & 0x80)
            {
                repcnt = (cmpinfo & 0x7f) + 3;
                compdata = *(sp++);
            }
            else
            {
                repcnt = (cmpinfo & 0x7f) + 1;
                compdata = -1;
            }
        }
        if (compdata >= 0)
        {
            *(dp++) = compdata & 0xff;
        }
        else
        {
            *(dp++) = *(sp++);
        }
        repcnt--;
    }
    
    
    return 0;
}



/******************************************************/

int mcmuncomp(uint8_t *dest, uint8_t *base, uint8_t *src)
{
    if (memcmp(src, "MCM", 4)) return -2;
    
    // uint32_t totalsize = get32(src+4);
    uint32_t chunksize = get32(src+8);
    uint32_t chunks = get32(src+0xc);
    
    uint8_t *buf = malloc(chunksize);
    
    for (int chunk = 0; chunk < chunks; chunk++)
    {
        uint8_t *compdest = dest + chunk*chunksize;
        for (int layer = 0; layer < 4; layer++)
        {
            uint8_t cmode = src[0x10+layer];
            
            uint8_t *compsrc;
            if (!layer)
            {
                compsrc = base + get32(src + 0x14 + chunk*4)-MAPBASE;
                if (!cmode) memcpy(compdest, compsrc, chunksize);
            }
            else
            {
                memcpy(buf, compdest, chunksize);
                compsrc = buf;
            }
            
            if (!cmode) break;
            
            int status;
            
            switch(cmode)
            {
                case 1:
                    status = rluncomp(compdest, compsrc);
                    break;
                case 2:
                    status = lz77uncomp(compdest, compsrc);
                    break;
                case 3:
                    status = huffuncomp(compdest, compsrc);
                    break;
                case 4:
                    status = diff8bitunfilter(compdest, compsrc);
                    break;
                case 5:
                    status = diff16bitunfilter(compdest, compsrc);
                    break;
                default:
                    status = -4;
                    break;
            }
            
            if (status) goto fail;
            
        }
    }
    
fail:
    free(buf);
    
    return 0;
}


size_t getmcmsize(uint8_t *p)
{
	return get32(p+4);
}

size_t getmcmbufsize(uint8_t *p)
{
  uint32_t totalsize = get32(p+4);
  uint32_t chunksize = get32(p+8);
  uint32_t chunks = get32(p+0xc);
  
  return max(totalsize,chunksize*chunks);
}


