#include "block_list.h"

/* takes up some room in the allocated block */
struct blist_block {
	struct list_head list;
	size_t head, tail;
	size_t size;

	unsigned char __data[];
};
#define BLOCK_OVERHEAD offsetof(struct elq_block, __data[0])

struct block_list {
	/* keep track of the used portion of blocks to guess at optimal block
	 * allocation sizes */
	size_t block_size_initial,
	       block_size_largest,
	       /* when a block is dequeued, we add it to this and divide by 2
		*/
	       block_size_weighted_average,
	       block_size_next;

	size_t elem_size; /* TODO: allow variable size elements */
	size_t block_ct;
	struct list_head blocks;
};

#define BL_EMPTY_BLOCK(size) (elq_t){ .head = 0, .tail = 0, .size = size }
#define BL_MIN_INITIAL_BLOCK_SIZE 64
#define BL_MIN_BLOCK_SIZE 64

#if BL_MIN_INITIAL_BLOCK_SIZE < BL_BLOCK_OVERHEAD
# error
#endif
#if ELQ_MIN_BLOCK_SIZE < ELQ_BLOCK_OVERHEAD
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
		fst_sz = ELQ_EMPTY_SIZE + min_init_blk_sz;
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
