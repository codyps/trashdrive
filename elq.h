#ifndef ELQ_H_
#define ELQ_H_

typedef struct elastic_queue elq_t;
elq_t *elq_create(size_t elem_size);

#endif
