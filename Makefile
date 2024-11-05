# Compiler
CC = gcc

# #OBJS specifies which files to compile as part of the project
OBJS = src/main.c

#OBJ_NAME specifies the name of our exectuable
OBJ_NAME = main

#This is the target that compiles our executable
all : $(OBJS)
	$(CC) $(OBJS) -ggdb -Wall -o $(OBJ_NAME)
