all::

TARGETS=index

src-tommy = $(wildcard tommyds/*.c)
obj-tommy = $(src-tommy:.c=.o)

ifndef V
	QUIET_SUBMAKE  = @ echo '  MAKE ' $@;
endif

ccan : FORCE
	$(QUIET_SUBMAKE)make --no-print-directory -C $@

ccan.clean :
	$(QUIET_SUBMAKE)make --no-print-directory -C $(@:.clean=) clean

dirclean : clean ccan.clean

ALL_CFLAGS += -Dtommy_inline="static inline" -I. -Iccan -std=gnu1x -pthread
ALL_LDFLAGS += -pthread -lrt -lccan -Lccan
obj-index = index.o block_list.o tommyds/tommyhashlin.o tommyds/tommylist.o
index : ccan

include base.mk

