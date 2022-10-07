CC ?= clang

MAIN := main
EXECUTABLE_FILE_NAME := ConcordBot

CFLAGS := -pthread -Wall -Wextra -Wpedantic -O2
LDFLAGS := -ldiscord -lcurl -lsqlite3

$(MAIN): $(MAIN).c
	$(CC) $(CFLAGS) -o $(EXECUTABLE_FILE_NAME) $^ $(LDFLAGS)

clean:
	rm -rf $(MAIN)
	rm -rf $(MAIN)
	rm -rf $(OBJDIR)
