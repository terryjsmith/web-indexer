
CC := g++
CFLAGS=-Wall -g -Iinclude -I/usr/include/mysql -pthread
LDFLAGS=-lmysqlclient -lhiredis -lcrypto -lssl -pthread -lcares

# Link command:
fetcher: src/main.o src/url.o src/httprequest.o src/worker.o src/domain.o
	$(CC) $^ -o bin/$@ $(LDFLAGS)

robots: src/url.o src/httprequest.o test/robotstest.o
	$(CC) $^ -o bin/$@ $(LDFLAGS)

# Individual files
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Build both
all: fetcher
test: urls robots

clean:
	rm src/*.o test/*.o
