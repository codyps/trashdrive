TARGETS=index

ALL_CFLAGS += -Dtommy_inline="static inline"

obj-index = index.o block_list.o

include base.mk
