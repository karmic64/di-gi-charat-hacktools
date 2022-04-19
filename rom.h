#pragma once

#include <stdint.h>


/* address of rom data in GBA address space */
#define MAPBASE 0x8000000


#define min(a,b) ((a) < (b) ? (a) : (b))


/* endianness-independent data read/writing */
static uint16_t get16(uint8_t *p) { return (p[0] | (p[1] << 8)); }
static uint32_t get24(uint8_t *p) { return (p[0] | (p[1] << 8) | (p[2] << 16)); }
static uint32_t get32(uint8_t *p) { return (p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); }

static void write16(uint8_t *p, uint16_t v)
{
    p[0] = v & 0xff;
    p[1] = (v & 0xff00) >> 8;
}

static void write24(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xff;
    p[1] = (v & 0xff00) >> 8;
    p[2] = (v & 0xff0000) >> 16;
}

static void write32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xff;
    p[1] = (v & 0xff00) >> 8;
    p[2] = (v & 0xff0000) >> 16;
    p[3] = (v & 0xff000000) >> 24;
}