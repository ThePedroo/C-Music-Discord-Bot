CC ?= clang

MAIN := main.c
EXECUTABLE_FILE_NAME := ./ConcordBot

DFLAGS := -Ofast
CFLAGS := -Wall -Wextra -Wpedantic -Ofast
LDFLAGS := -std=gnu99 -ldiscord -lcurl -pthread -I /usr/include/postgresql -lpq

all:
	$(CC) $(CFLAGS) -o $(EXECUTABLE_FILE_NAME) $(MAIN) $(LDFLAGS)

debug:
	$(CC) -Wall -Wextra -Wpedantic -g -o $(EXECUTABLE_FILE_NAME) $(MAIN) $(LDFLAGS)

clean:
	rm -rf $(EXECUTABLE_FILE_NAME)

run:
	$(EXECUTABLE_FILE_NAME)