#ifndef BLOCK_LIST_H_
#define BLOCK_LIST_H_

#include "penny/list.h"

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

void blist_init(struct block_list *bl, size_t elem_size);

struct blist_block *blist_dequeue_block(struct block_list *bl);
void blist_enqueue_block(struct block_list *bl, struct blist_block *b);




void *blist_dequeue_elem(void);

/* returns a pointer to memory which @elem_size bytes can be written into
 * It also makes readers immediately believe their is a new element in the
 * queue, even before data is written to the location pointed to by the
 * returned pointer. */
void *blist_enqueue_elem(void);


#endif
