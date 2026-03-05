#ifndef BLOCK_H
#define BLOCK_H
#include <stdint.h>
#include <stdio.h>
typedef struct {
	int16_t pointer;
	int16_t length;
	int16_t block_pointer;
	int16_t depth;
}block_t;

int block_init(block_t* block, unsigned int length, unsigned int depth);
void block_increase(block_t* block);
void block_decrease(block_t * block);
void block_print(block_t* block);
#endif