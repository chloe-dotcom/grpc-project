CC = g++
CFLAGS = -Wall -Wextra -O2 -std=c++17

HB := $(shell brew --prefix)
SERVER_CFLAGS = -I$(HB)/include
SERVER_LDFLAGS = -L$(HB)/lib -lprom -lpromhttp -lmicrohttpd -lpthread -Wl,-rpath,$(HB)/lib
TARGETS = server client

.PHONY: all clean run-server run-client

all: $(TARGETS)
server: server.cpp
	$(CC) $(CFLAGS) $(SERVER_CFLAGS) -o $@ $< $(SERVER_LDFLAGS)

client: client.cpp
	$(CC) $(CFLAGS) -o $@ $<

run-server: server
	DYLD_LIBRARY_PATH=$(HB)/lib ./server

run-client: client
	./client

clean:
	rm -f $(TARGETS)
