all::

TARGETS=index

src-tommy = $(wildcard tommyds/tommyds/*.c)
obj-tommy = $(src-tommy:.c=.o)

ALL_CFLAGS  += -Dtommy_inline="static inline" -I. -std=gnu1x -pthread -Itommyds
ALL_LDFLAGS += -pthread -lrt
obj-index = index.o sync_path.o	tommyds/tommyds/tommyhashlin.o tommyds/tommyds/tommylist.o

include base.mk
include base-ccan.mk

