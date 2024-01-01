#pragma once

#include <stdlib.h>
#include <stdint.h>


int huffuncomp(uint8_t *dest, uint8_t *src);
int lz77uncomp(uint8_t *dest, uint8_t *src);
int rluncomp(uint8_t *dest, uint8_t *src);
int diff8bitunfilter(uint8_t *dest, uint8_t *src);
int diff8bitunfilter(uint8_t *dest, uint8_t *src);


int mcmuncomp(uint8_t *dest, uint8_t *base, uint8_t *src);
size_t getmcmsize(uint8_t *p);
size_t getmcmbufsize(uint8_t *p);
