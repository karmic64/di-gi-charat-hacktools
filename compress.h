
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>


#define min(a,b) ((a) < (b) ? (a) : (b))

uint16_t get16(uint8_t *p) { return (p[0] | (p[1] << 8)); }
uint32_t get24(uint8_t *p) { return (p[0] | (p[1] << 8) | (p[2] << 16)); }
uint32_t get32(uint8_t *p) { return (p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); }

void write16(uint8_t *p, uint16_t v)
{
    p[0] = v & 0xff;
    p[1] = (v & 0xff00) >> 8;
}

void write24(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xff;
    p[1] = (v & 0xff00) >> 8;
    p[2] = (v & 0xff0000) >> 16;
}

void write32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xff;
    p[1] = (v & 0xff00) >> 8;
    p[2] = (v & 0xff0000) >> 16;
    p[3] = (v & 0xff000000) >> 24;
}




/************************* huffman *******************/

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

int huffcomp(uint8_t *dest, uint8_t *src, size_t size)
{
    if (!size) return -1;
    while (size % 4) size++;
    if (size > 0xffffff) return -1;
    
    write24(dest+1, size);
    
    uintmax_t freqs[0x100];
    memset(freqs, 0, sizeof(freqs));
    
    uint8_t *sp = src;
    uint8_t *se = src+size;
    
    for (uint8_t *p = sp; p < se; p++)
        freqs[*p]++;
    
    unsigned uniques = 0;
    for (int i = 0; i < 0x100; i++)
        if (freqs[i]) uniques++;
    
    /* prefer 8-bit packing, but if there are too many unique values we may have to settle for 4-bit */
    unsigned bits = uniques < 0x40 ? 8 : 4;
    *dest = 0x20 | bits;
    unsigned max = 1 << bits;
    
    if (bits != 8)
    {
        memset(freqs, 0, sizeof(freqs));
        for (uint8_t *p = sp; p < se; p++)
        {
            freqs[((*p) >> 4) & 0x0f]++;
            freqs[((*p)) & 0x0f]++;
        }
    }
    
    struct node
    {
        uint8_t data; /* only relevant if leaf node */
        uintmax_t freq;
        
        struct node *left;
        struct node *right;
    };
    
    struct node nullnode = {0, ULLONG_MAX, NULL, NULL};
    
    int node_cmp(const void *a, const void *b)
    {
        uintmax_t af = ((const struct node*)a)->freq;
        uintmax_t bf = ((const struct node*)b)->freq;
        
        return (af > bf) - (af < bf);
    }
    
    int node_ptr_cmp(const void *a, const void *b)
    {
        return node_cmp(*(const struct node**)a, *(const struct node**)b);
    }
    
    struct node nodetbl[0x400];
    
    unsigned nodes = 0;
    for (int j = 0; j < max; j++)
    {
        if (freqs[j])
        {
            nodetbl[nodes].data = j;
            nodetbl[nodes].freq = freqs[j];
            nodetbl[nodes].left = NULL;
            nodetbl[nodes].right = NULL;
            
            nodes++;
        }
    }
    
    if (!nodes) return -1;
    
    struct node *heap[nodes];
    
    unsigned heapsize = nodes;
    
    for (int i = 0; i < heapsize; i++)
        heap[i] = &nodetbl[i];
    
    unsigned heapleft = nodes;
    
    while (heapleft > 1)
    {
        qsort(heap, min(heapleft+1,heapsize), sizeof(*heap), node_ptr_cmp);
        
        nodetbl[nodes].freq = heap[0]->freq + heap[1]->freq;
        nodetbl[nodes].left = heap[0];
        nodetbl[nodes].right = heap[1];
        
        heap[0] = &nodetbl[nodes++];
        heap[1] = &nullnode;
        
        heapleft--;
    }
    
    
    /* now actually create the data */
    /* the parent node is heap[0] */
    
    /* traverse through the tree to get the depths/values for every byte, and make the table */
    unsigned depths[0x100];
    unsigned vals[0x100];
    memset(depths, 0, sizeof(bits));
    
    memset(dest+4, 0, size-4);
    uint8_t *dp = dest+6;
    
    int fail = 0;
    
    void traverse(struct node *n, unsigned depth, unsigned val, uint8_t *here)
    {
        if (!n->left && !n->right)
        {
            depths[n->data] = depth;
            vals[n->data] = val;
            
            *here = n->data;
        }
        else
        {
            uint8_t *next = dp;
            uint8_t offs = (((intptr_t)next - ((intptr_t)here & ~1)) - 2) / 2;
            if (offs > 0x3f)
            {
                fail = -3;
                return;
            }
            else if (dp >= dest+size)
            {
                fail = -2;
                return;
            }
            
            uint8_t leftdata = !n->left->left && !n->left->right;
            uint8_t rightdata = !n->right->left && !n->right->right;
            
            *here = offs | (leftdata ? 0x80 : 0x00) | (rightdata ? 0x40 : 0x00);
            
            dp += 2;
            
            if (n->left) traverse(n->left, depth+1, val, next);
            if (n->right) traverse(n->right, depth+1, val | (1 << depth), next+1);
        }
    }
    
    traverse(heap[0], 0, 0, dest+5);
    
    if (fail) return fail;
    
    while ((intptr_t)dp & 3) dp++;
    *(dest+4) = (dp-(dest+4))/2-1;
    
    
    uint8_t bit = 0;
    void writeval(unsigned bits, unsigned val)
    {
        for (unsigned b = 0; b < bits; b++)
        {
            if (val & (1 << b)) *(dp+((bit/8) ^ 3)) |= 0x80 >> (bit & 7);
            
            if (++bit >= 32)
            {
                bit = 0;
                dp += 4;
            }
        }
    }
    
    while (sp < se)
    {
        /* don't bother any more if we inflated the file */
        if (dp >= dest+size) return -2;
        if (bits == 8)
        {
            uint8_t v = *(sp++);
            writeval(depths[v], vals[v]);
        }
        else
        {
            uint8_t v = *(sp++);
            writeval(depths[v & 0x0f], vals[v & 0x0f]);
            writeval(depths[(v>>4) & 0x0f], vals[(v>>4) & 0x0f]);
        }
    }
    
    
    if (bit) dp += 4;
    return dp-dest;
}





/*********************** LZ77 ***********************/

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

int lz77comp(uint8_t *dest, uint8_t *src, size_t size)
{
    int count(uint8_t *s1, uint8_t *s2, int max)
    {
        int i = 0;
        
        while (i < max && (*(s1++) == *(s2++))) i++;
        
        return i;
    }
    
    if (!size) return -1;
    while (size % 4) size++;
    if (size > 0xffffff) return -1;
    
    *dest = 0x10;
    write24(dest+1, size);
    
    uint8_t *sp = src;
    uint8_t *se = src+size;
    
    uint8_t *dp = dest+4;
    uint8_t *dblock;
    
    uint8_t block = 8;
    
    void addblock(uint8_t type, uint16_t data)
    {
        if (++block >= 8)
        {
            block = 0;
            
            dblock = dp;
            *dblock = 0;
            
            dp = dblock + 1;
        }
        
        if (type)
        {
            write16(dp, data);
            dp += 2;
            
            *dblock |= (0x80 >> block);
        }
        else
        {
            *(dp++) = data & 0xff;
        }
    }
    
    while (sp < se)
    {
        /* don't bother any more if we inflated the file */
        if (dp >= dest+size) return -2;
        
        uint16_t windowsize = min(sp-src, 0x1000);
        int maxmatch = 0;
        uint8_t *maxptr = NULL;
        for (uint8_t *p = sp-1; p >= sp-windowsize; p--)
        {
            int matches = count(p, sp, min(sp-p, 0x12));
            if (matches > maxmatch)
            {
                maxmatch = matches;
                maxptr = p;
                
                /* we can't get any better than 0x12 bytes */
                if (maxmatch == 0x12) break;
            }
        }
        
        if (maxmatch < 3)
        {
            if (!maxmatch) maxmatch = 1;
            for (int i = 0; i < maxmatch; i++)
                addblock(0, *(sp++));
        }
        else
        {
            /* ptr = dest-disp-1 */
            uint16_t disp = sp-maxptr-1;
            
            uint16_t data = (disp & 0xff) << 8;
            data |= (maxmatch-3) << 4;
            data |= ((disp & 0xf00) >> 8);
            
            addblock(1, data);
            
            sp += maxmatch;
        }
        
    }
    
    return dp-dest;
}




/******************* diff8bit *****************/

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



/****************** diff16bit *******************/

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




/**************** RLE *****************/

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
            /* don't bother any more if we inflated the file */
            if (dp >= dest+size)
            {
                free(runtbl);
                return -2;
            }
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


size_t getmcmbufsize(uint8_t *p)
{
  uint32_t totalsize = get32(p+4);
  uint32_t chunksize = get32(p+8);
  
  return (totalsize < chunksize) ? chunksize : totalsize;
}


