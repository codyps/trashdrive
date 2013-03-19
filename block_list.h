#ifndef BLOCK_LIST_H_
#define BLOCK_LIST_H_

struct blist_block;
typedef struct block_list blist_t;

void blist_init(size_t elem_size);
struct blist_block *blist_dequeue_block(struct block_list *bl);

void *blist_dequeue_elem(void);

/* returns a pointer to memory which @elem_size bytes can be written into
 * It also makes readers immediately believe their is a new element in the
 * queue, even before data is written to the location pointed to by the
 * returned pointer. */
void *blist_enqueue_elem(void);

#endif
