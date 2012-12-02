
CC := g++
CFLAGS=-Wall -g -Iinclude
LDFLAGS=-lcurl -lmysqlclient -lhiredis

# Link command:
crawler: src/main.o src/url.o src/httprequest.o
	$(CC) $^ -o bin/$@ $(LDFLAGS)

urltest: test/urltest.o src/url.o
	$(CC) $^ -o test/$@ $(LDFLAGS)

# Individual files
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Build both
all: crawler test
test: urltest

clean:
	rm src/*.o test/*.o
