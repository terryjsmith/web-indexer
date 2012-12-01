
CC := g++
CFLAGS=-Wall -g -Iinclude -lpthread -L/usr/local/lib -lcurl -lmysqlclient

# Link command:
crawler: main.cpp databaseconnection.o crawler.o worker.o httprequest.o url.o patternmatch.o *.o
  $(CC) $(CFLAGS) -lpthread main.cpp  /usr/local/ssl/lib/libcrypto.a *.o -o $@

# Individual files
%.o: %.cpp
  $(CC) $(CFLAGS) -c $< -o $@

# Build both
all: crawler

clean:
  rm *.o
