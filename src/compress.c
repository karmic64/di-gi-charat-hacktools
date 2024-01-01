
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "compress.h"

#include "rom.h"



/************************* huffman *******************/



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

