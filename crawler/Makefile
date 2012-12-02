
CC := g++
CFLAGS=-Wall -g -Iinclude -lpthread -lcurl -lmysqlclient -lhiredis

# Link command:
crawler: src/main.cpp src/url.cpp src/parser.cpp
  $(CC) $(CFLAGS) *.o -o $@

# Individual files
%.o: %.cpp
  $(CC) $(CFLAGS) -c $< -o $@

# Build both
all: crawler

clean:
  rm *.o
