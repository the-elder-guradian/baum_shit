all: test_listener

CC = g++
CFLAGS = -g -ggdb -Wall -Wextra -pedantic -pthread
LFLAGS = 

CPP_STANDARD = c++17

INCLUDES = -I. -I/usr/include

LIBRARY_DIRECTORIES = -L. -L/usr/lib
LIBRARIES = 

.PHONY: clear

test_listener: client.cpp listener.cpp main.cpp
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ -std=$(CPP_STANDARD) $(INCLUDES) $(LIBRARY_DIRECTORIES) $(LIBRARIES) $^

clear:
	rm test_listener
