# Simple Makefile for Filesystem Cache Inspector

CC = gcc
CFLAGS = -Wall -O2
TARGET = cache_inspector
SRC = cache_inspector.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) /usr/bin/bash

clean:
	rm -f $(TARGET)
