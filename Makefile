TARGETS=index

src-tommy = $(wildcard tommyds/*.c)
obj-tommy = $(src-tommy:.c=.o)

ALL_CFLAGS += -Dtommy_inline="static inline" -I. -std=gnu1x -pthread
ALL_LDFLAGS += -pthread -lrt
obj-index = index.o block_list.o tommyds/tommyhashlin.o tommyds/tommylist.o

include base.mk
