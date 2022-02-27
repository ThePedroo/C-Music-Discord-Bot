CC ?= clang

SRCDIR := src
OBJDIR := obj
INCLUDEDIR := include

SRC := $(wildcard $(SRCDIR)/*.c $(SRCDIR)/**/*.c)
OBJS := $(SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

MAIN := main

CFLAGS := -pthread -Wall -Wextra -Wpedantic -O3 -I$(INCLUDEDIR)
LDFLAGS := -ldiscord -lcurl -lcrypto -lm -lsqlite3

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $< $(LDFLAGS)

all: $(MAIN)

$(MAIN): $(MAIN).c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): | $(OBJDIR)

$(OBJDIR):
	mkdir -p $(dir $(OBJS))

echo:
	@ echo SRC: $(SRC)
	@ echo OBJS: $(OBJS)

clean:
	rm -rf $(MAIN)
	rm -rf $(OBJDIR)
