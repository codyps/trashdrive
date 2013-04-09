#include <stddef.h>
#include <penny/math.h>
#include <penny/check.h>
#include <unistd.h>
#include "block_list.h"

#define BASE_SIZE sizeof(struct block_list)
/* takes up some room in the allocated block */
struct blist_block {
	struct list_node list;
	size_t size;
	size_t head, tail;

	unsigned char __data[];
};
#define BLOCK_OVERHEAD offsetof(struct blist_block, __data[0])
#define EMPTY_BLOCK(size) (struct block_list){ .head = 0, .tail = 0, .size = size }

static size_t pagesize_get(void)
{
	static size_t pagesize = -1;
	if (pagesize > 0)
		return pagesize;

	pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize == -1)
#ifdef PAGE_SIZE
		pagesize = PAGE_SIZE;
#else
		pagesize = 4096;
#endif

	return pagesize;
}

static size_t guess_initial_block_size(size_t elem_size)
{
	size_t elems_per_pagesize_block =
		DIV_OR_ZERO(pagesize_get() - BLOCK_OVERHEAD, elem_size);
	if (elems_per_pagesize_block)
		/* FIXME: multiply elem_size if it makes sense to,
		 * for example, if BLOCK_OVERHEAD + 2*elem_size makes us closer
		 * to a pagesize multiple, do it. */
		return BLOCK_OVERHEAD + elem_size;
	return pagesize_get();
}

void blist_init(struct block_list *bl, size_t elem_size)
{
	*bl = (typeof(*bl)) {
		.block_size_next = guess_initial_block_size(elem_size),

		.elem_size = elem_size,
		.block_ct = 0,
	};
}

void blist_enqueue_block(struct block_list *bl, struct blist_block *b)
{
	list_add_tail(&bl->blocks, &b->list);
}

struct blist_block *blist_dequeue_block(struct block_list *bl)
{
	return list_pop(&bl->blocks, struct blist_block, list);
}
