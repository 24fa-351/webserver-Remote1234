# Name of the output executable
TARGET = webserver

# Compiler to use
CC = gcc

# Compiler flags
CFLAGS = -pthread -Wall

# Default target to build the project
all: $(TARGET)

# Rule to build the executable
$(TARGET): webserver.c
	$(CC) $(CFLAGS) -o $(TARGET) webserver.c

# Clean up generated files
clean:
	rm -f $(TARGET)
