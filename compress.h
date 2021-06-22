

#include <stdint.h>


#define get16(p) ((*(p)) | ((*(p+1)) << 8))
#define get24(p) ((*(p)) | ((*(p+1)) << 8) | ((*(p+2)) << 16))
#define get32(p) ((*(p)) | ((*(p+1)) << 8) | ((*(p+2)) << 16) | ((*(p+3)) << 24))

void write16(uint8_t *p, uint16_t v)
{
    *p = v & 0xff;
    *(p+1) = (v & 0xff00) >> 8;
}

void write24(uint8_t *p, uint32_t v)
{
    *p = v & 0xff;
    *(p+1) = (v & 0xff00) >> 8;
    *(p+2) = (v & 0xff0000) >> 16;
}

void write32(uint8_t *p, uint32_t v)
{
    *p = v & 0xff;
    *(p+1) = (v & 0xff00) >> 8;
    *(p+2) = (v & 0xff0000) >> 16;
    *(p+3) = (v & 0xff000000) >> 24;
}






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

int diff8bitfilter(uint8_t *dest, uint8_t *src, size_t size)
{
    if (!size) return -1;
    while (size % 4) size++;
    if (size > 0xffffff) return -1;
    
    *dest = 0x81;
    write24(dest+1, size);
    
    uint8_t *sp = src;
    uint8_t *se = src+size;
    uint8_t *dp = dest+4;
    uint8_t prvdata = 0;
    
    while (sp < se)
    {
        uint8_t data = *(sp++);
        *(dp++) = data - prvdata;
        prvdata = data;
    }
    
    
    return dp-dest;
}




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

int diff16bitfilter(uint8_t *dest, uint8_t *src, size_t size)
{
    if (!size) return -1;
    while (size % 4) size++;
    if (size > 0xffffff) return -1;
    
    *dest = 0x82;
    write24(dest+1, size);
    
    uint8_t *sp = src;
    uint8_t *se = src+size;
    uint8_t *dp = dest+4;
    uint16_t prvdata = 0;
    
    while (sp < se)
    {
        uint16_t data = get16(sp);
        sp += 2;
        write16(dp, data - prvdata);
        dp += 2;
        prvdata = data;
    }
    
    
    return dp-dest;
}






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

int rlcomp(uint8_t *dest, uint8_t *src, size_t size)
{
    if (!size) return -1;
    while (size % 4) size++;
    if (size > 0xffffff) return -1;
    
    *dest = 0x30;
    write24(dest+1, size);
    
    uint8_t *sp = src;
    uint8_t *se = src+size;
    
    /* create the run-length table */
    uint8_t *runtbl = malloc(size*2+1);
    uint8_t *rp = runtbl;
    
    uint8_t runcnt = 0;
    
    while (sp < se)
    {
        uint8_t data = *(sp++);
        runcnt++;
        /* don't count past 0x82 since that's the max run length representable by the format */
        if (sp == se || data != *sp || runcnt == 0x82)
        {
            *(rp++) = runcnt;
            *(rp++) = data;
            
            runcnt = 0;
        }
    }
    
    uint8_t *re = rp;
    
    /* now build the actual data */
    uint8_t *dp = dest+4;
    rp = runtbl;
    uint8_t *prv = runtbl; /* the next run that must be written to file */
    uint8_t unclen = 0; /* current uncompressed data length */
    while (rp <= re)
    {
        uint8_t *oldrp = rp;
        uint8_t cnt = *(rp++);
        uint8_t val = *(rp++);
        uint8_t newunclen = unclen + cnt;
        if (cnt >= 3 || newunclen > 0x80 || rp == re)
        { /* compressable run length/uncompressed length overflow/end of file */
            /* first handle any uncompressed runs */
            if (unclen)
            {
                *(dp++) = unclen-1;
                while (prv < oldrp)
                {
                    uint8_t cnt = *(prv++);
                    uint8_t val = *(prv++);
                    
                    for (int i = 0; i < cnt; i++)
                        *(dp++) = val;
                }
                unclen = cnt;
            }
            /* now handle any compressable run */
            if (cnt >= 3)
            {
                *(dp++) = 0x80 | (cnt-3);
                *(dp++) = val;
                prv = rp;
                unclen = 0;
            }
        }
        else
        {
            /* otherwise just keep up the count */
            unclen = newunclen;
        }
    }
    
    
    free(runtbl);
    
    return dp-dest;
}





/******************************************************/
#define MAPBASE 0x8000000

int mcmuncomp(uint8_t *dest, uint8_t *base, uint8_t *src)
{
    if (memcmp(src, "MCM", 4)) return -2;
    
    uint32_t totalsize = get32(src+4);
    uint32_t chunksize = get32(src+8);
    uint32_t chunks = get32(src+0xc);
    
    // if (chunks * chunksize > totalsize) return -3;
    
    memset(dest, 0, totalsize);
    
    uint8_t *dsrcbuf = malloc(chunksize*2);
    uint8_t *ddestbuf = malloc(chunksize*2);
    
    int status = 0;
    
    for (int chunk = 0; chunk < chunks; chunk++)
    {
        uint8_t *chunkbase = base+get32(src+0x14+(chunk*4))-MAPBASE;
        uint8_t *chunkdest = dest+(chunk*chunksize);
        memcpy(dsrcbuf, chunkbase, chunksize*2);
        for (int i = 0; i < 4; i++)
        {
            uint8_t cmptype = *(src+0x10+i);
            if (!cmptype) continue;
            if (cmptype > 5)
            {
                status = -4;
                goto fail;
            }
            switch (cmptype)
            {
                case 1:
                    status = rluncomp(ddestbuf, dsrcbuf);
                    break;
                case 2:
                    status = lz77uncomp(ddestbuf, dsrcbuf);
                    break;
                case 3:
                    status = huffuncomp(ddestbuf, dsrcbuf);
                    break;
                case 4:
                    status = diff8bitunfilter(ddestbuf, dsrcbuf);
                    break;
                case 5:
                    status = diff16bitunfilter(ddestbuf, dsrcbuf);
                    break;
                default:
                    status = -4;
                    break;
            }
            if (status) goto fail;
            memcpy(dsrcbuf, ddestbuf, chunksize*2);
        }
        memcpy(chunkdest, ddestbuf, (chunk < chunks-1) ? chunksize : (totalsize % chunksize));
    }
    
fail:
    free(dsrcbuf);
    free(ddestbuf);
    return status;
}




