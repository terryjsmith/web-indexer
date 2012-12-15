
CC := g++
CFLAGS=-Wall -g -Iinclude -I/usr/local/include -I/usr/include/mysql
LDFLAGS=-L/usr/local/lib -lmysqlclient -lhiredis -lcrypto -lhtmlcxx

# Link command:
parser: src/main.o src/url.o 
	$(CC) $^ -o bin/$@ $(LDFLAGS)

urltest: test/urltest.o src/url.o
	$(CC) $^ -o bin/$@ $(LDFLAGS)

# Individual files
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Build both
all: parser

clean:
	rm src/*.o test/*.o
