CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)gcc
RM = rm -f

MAKEFLAGS += -Rr --no-print-directory

ifndef V
	QUIET_CC = @ echo '    ' CC $@;
	QUIET_LD = @ echo '    ' LD $@;
endif

.PHONY: all
all: build

LIBUV_DIR = ../libuv
LIBUV = $(LIBUV_DIR)/uv.a
LIBUV_LDFLAGS = -lm -lrt -pthread

$(LIBUV) :
	$(MAKE) -C $(LIBUV_DIR)

TARGETS = trashdrive
trashdrive: trashdrive.c.o $(LIBUV)

srcdir = .
VPATH = $(srcdir)

CFLAGS           += -ggdb
override CFLAGS  += -Wall -Wextra -Werror -pipe -I$(LIBUV_DIR)/include
LDFLAGS          += -Wl,--as-needed -O2
override LDFLAGS += $(LIBUV_LDFLAGS)

.PHONY: rebuild
rebuild: | clean build

.PHONY: build
build: $(TARGETS)

%.c.o : %.c
	$(QUIET_CC)$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(TARGETS) :
	$(QUIET_LD)$(LD) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	$(RM) $(TARGETS) *.d *.o

.PHONY: test-dirs
test-dirs:
	$(MKDIR) test test.cache

.PHONY:
test: $(TARGETS)
	./trashdrive test test.cache


-include $(wildcard *.d)
