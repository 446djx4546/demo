#include "Block.h"
  
int block_init(block_t* block, unsigned int length, unsigned int depth) {
	if (block == NULL) {
		return -1;
	}
	block->pointer = 0;
	block->block_pointer = 0;
	if (length > depth) {
		block->length = depth;
	}
	else {
		block->length = length;
	}
	block->depth = depth;
	return 0;
}

void block_increase(block_t* block) {
	if (block->pointer < block->block_pointer + block->length - 1) {
		block->pointer++;
	}
	else if (block->block_pointer + block->length < block->depth) {
		block->pointer++;
		block->block_pointer++;
	}
}

void block_decrease(block_t* block) {
	if (block->pointer > block->block_pointer) {
		block->pointer--;
	}
	else if (block->block_pointer > 0) {
		block->pointer--;
		block->block_pointer--;
	}
}

void block_print(block_t* block) {
	printf("\n%d\n%d\n%d\n%d\n", block->pointer, block->block_pointer, block->length, block->depth);
}