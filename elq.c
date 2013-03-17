#include "elq.h"

/* takes up some room in the allocated block */
struct elq_block {
	/* FIXME: is a smaller type appropriate? */
	size_t head, tail;
	size_t size;

	/* followed by elements of elem_size indicated in the parent elq_t */
	unsigned char __data[];
};
#define ELQ_BLOCK_OVERHEAD offsetof(struct elq_block, __data[0])

struct elastic_queue {
	size_t block_sz;
	size_t elem_size; /* TODO: allow variable size elements */
	size_t block_ct;  /* TODO: allow chained blocks */
	struct elq_block *blocks[64];
	/* only need 64 blocks...,
	 * use the remaining pagesize as the first block */
	struct elq_block first_block;
};
#define ELQ_EMPTY_SIZE offsetof(elq_t, first_block)

/* If we allocate all blocks of the same size:
 * typical byte count on a 64 bit system with a 4096 byte page size
 * blocks = (4096 - 4 * 3) / 4 = 1021
 * bytes_per_block = (4096 - 4 * 2) = 4088
 * bytes = blocks * bytes_per_block = 1021 * 4088 = 4173848 = 4076kB
 *
 * Pitiful - how can we fix this?
 * - Blocks of multiple pages?
 * - Hugepages?
 *
 * The real problem is linear growth: we want exponential growth.
 * - use a larger block each time.
 * - double the size as we get larger.
 * - deallocate/stop using smaller blocks first
 *   - this bit requires tricky accounting or an extra per-block field, keep it
 *     as a TODO.
 *
 * Double the block size with each allocation.
 * same number of blocks.
 * total bytes = 4096 * sum(x, 0, blocks[1021]-1, 2^x)
 *             = (1 - 2^1021) / (1 - 1021)
 *             ~= 2056.0313725490 * 1024^100
 *             = far, far, to many to ever be useful, meaning in the common
 *               case we waste many bytes.
 * The final block size = 4096 * 2^1020, which is also enormous enough to not
 * be useful at all.
 *
 * - Is doubling block size the wrong growth function?
 *   - what if we scale the limits of the function based on the address space
 *     or some other limit?
 *     - could make the calculation less than fast (and it needs to be fast).
 * - Is our initial block size (= pagesize = 4096) too large?
 * - Do we have too many blocks?
 */
#define ELQ_EMPTY_BLOCK(size) (elq_t){ .head = 0, .tail = 0, .size = size }
#define ELQ_MIN_INITIAL_BLOCK_SIZE 64
#define ELQ_MIN_BLOCK_SIZE 64

#if ELQ_MIN_INITIAL_BLOCK_SIZE < ELQ_BLOCK_OVERHEAD
# error
#endif
#if ELQ_MIN_BLOCK_SIZE < ELQ_BLOCK_OVERHEAD
# error
#endif
elq_t *elq_create(size_t elem_size)
{
	size_t pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize == -1)
#ifdef PAGE_SIZE
		pagesize = PAGE_SIZE;
#else
		pagesize = 4096;
#endif

	size_t min_init_blk_sz = ELQ_BLOCK_OVERHEAD + MAX(ELQ_MIN_INITIAL_BLOCK_SIZE, elem_size);
	size_t min_blk_sz = ELQ_BLOCK_OVERHEAD + MAX(ELQ_MIN_BLOCK_SIZE, elem_size);
	size_t fst_sz;
	size_t blocksize;
	if (ELQ_EMPTY_SIZE + min_init_blk_sz > pagesize) {
		fst_sz = ELQ_EMPTY_SIZE + min_init_blk_sz;
		blocksize = min_blk_sz;
	} else {
		fst_sz = pagesize;
		blocksize = MAX(pagesize, ELQ_BLOCK_OVERHEAD + elem_size);
	}

	elq_t *q = malloc(fst_sz);
	if (!q)
		return NULL;

	*q = (typeof(*q)) {
		.block_sz = blocksize,
		.elem_size = elem_size,
		.block_ct = 1,

		.blocks = {
			&q->first_block
		};

		.first_block = EQL_EMPTY_BLOCK(fst_sz - ELQ_EMPTY_SIZE);
	};

	return q;
}

void *elq_insert_get();
elq_insert_done();
