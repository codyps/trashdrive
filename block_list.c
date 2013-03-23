#include "block_list.h"

#define BASE_SIZE sizeof(struct block_list)
/* takes up some room in the allocated block */
struct blist_block {
	struct list_head list;
	size_t head, tail;
	size_t size;

	unsigned char __data[];
};
#define BLOCK_OVERHEAD offsetof(struct blist_block, __data[0])

#define EMPTY_BLOCK(size) (blist_t){ .head = 0, .tail = 0, .size = size }
#define MIN_INITIAL_BLOCK_SIZE 64
#define MIN_BLOCK_SIZE 64

#if MIN_INITIAL_BLOCK_SIZE < BL_BLOCK_OVERHEAD
# error
#endif
#if MIN_BLOCK_SIZE < BLOCK_OVERHEAD
# error
#endif

void blist_init(blist_t *bl, size_t elem_size, size_t initial_block_elem_ct)
{
	size_t pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize == -1)
#ifdef PAGE_SIZE
		pagesize = PAGE_SIZE;
#else
		pagesize = 4096;
#endif

	size_t min_init_blk_sz = BLOCK_OVERHEAD + MAX(MIN_INITIAL_BLOCK_SIZE, elem_size);
	size_t min_blk_sz = BLOCK_OVERHEAD + MAX(MIN_BLOCK_SIZE, elem_size);
	size_t fst_sz;
	size_t blocksize;
	if (EMPTY_SIZE + min_init_blk_sz > pagesize) {
		fst_sz = BL_EMPTY_SIZE + min_init_blk_sz;
		blocksize = min_blk_sz;
	} else {
		fst_sz = pagesize;
		blocksize = MAX(pagesize, BLOCK_OVERHEAD + elem_size);
	}

	*bl = (typeof(*bl)) {
		.block_size_next = blocksize

		.elem_size = elem_size,
		.block_ct = 0,
	};
}

void *elq_insert_get();
elq_insert_done();
