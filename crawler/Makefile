
CC := g++
CFLAGS=-Wall -g -Iinclude
LDFLAGS=-lpthread -lcurl -lmysqlclient -lhiredis

# Link command:
crawler: src/main.o src/url.o src/httprequest.o
	$(CC) $< -o bin/$@ $(LDFLAGS)

# Individual files
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Build both
all: crawler

clean:
	rm src/*.o
