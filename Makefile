CC ?= clang

MAIN := main.c
EXECUTABLE_FILE_NAME := ConcordBot

CFLAGS := -Wall -Wextra -Wpedantic -Ofast
LDFLAGS := -ldiscord -lcurl -lsqlite3 -pthread

all:
	$(CC) $(CFLAGS) -o $(EXECUTABLE_FILE_NAME) $(MAIN) $^ $(LDFLAGS)

debug:
	CFLAGS="-O0 -g" $(CC) $(CFLAGS) -o $(EXECUTABLE_FILE_NAME) $(MAIN) $^ $(LDFLAGS)

clean:
	rm -rf $(MAIN)
