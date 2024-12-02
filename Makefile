# Compiler
CC = gcc

# Compiler arguments: pthreads, gdb debug symbols, treat all warnings as errors
CC_ARGS = -pthread -ggdb -Wall

# OBJS specifies which files to compile as part of the project
OBJS = src/client.c src/destinations.c src/shared.c src/server.c
# HEADERS specifies the header files
HEADERS = src/client.h src/destinations.h src/shared.h

# OBJ_NAME specifies the name of our exectuable
OBJ_NAME = main

# This is the target that compiles our executable
all : $(OBJS)
	$(CC) $(OBJS) $(HEADERS) $(CC_ARGS) -o $(OBJ_NAME)
