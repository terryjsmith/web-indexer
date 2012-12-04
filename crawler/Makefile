
CC := g++
CFLAGS=-Wall -g -Iinclude -I/usr/include/mysql -pthread
LDFLAGS=-lcurl -lmysqlclient -lhiredis -lssl -pthread

# Link command:
crawler: src/main.o src/url.o src/httprequest.o src/robotstxt.o src/site.o src/worker.o
	$(CC) $^ -o bin/$@ $(LDFLAGS)

urltest: test/urltest.o src/url.o
	$(CC) $^ -o test/$@ $(LDFLAGS)

fetchtest: test/fetchtest.o src/url.o src/httprequest.o
	$(CC) $^ -o test/$@ $(LDFLAGS)

robotstest: test/robotstest.o src/url.o src/httprequest.o src/robotstxt.o src/site.o
	$(CC) $^ -o test/$@ $(LDFLAGS)

# Individual files
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Build both
all: crawler test
test: urltest fetchtest

clean:
	rm src/*.o test/*.o
