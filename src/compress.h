#pragma once

#include <stdlib.h>
#include <stdint.h>


int huffcomp(uint8_t *dest, uint8_t *src, size_t size);
int lz77comp(uint8_t *dest, uint8_t *src, size_t size);
int diff8bitfilter(uint8_t *dest, uint8_t *src, size_t size);
int diff16bitfilter(uint8_t *dest, uint8_t *src, size_t size);
int rlcomp(uint8_t *dest, uint8_t *src, size_t size);