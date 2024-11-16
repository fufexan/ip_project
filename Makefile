# Compiler
CC = gcc

# OBJS specifies which files to compile as part of the project
CLIENT_OBJS = src/client.c src/destinations.c
# HEADERS specifies the header files
HEADERS = src/destinations.h

SERVER_OBJS = src/server.c

#OBJ_NAME specifies the name of our exectuable
OBJ_NAME = main

#This is the target that compiles our executable
# all : $(CLIENT_OBJS) $(SERVER_OBJS)
# 	$(CC) $(SERVER_OBJS) $(CLIENT_OBJS) $(HEADERS) -ggdb -Wall -o $(OBJ_NAME)

client : $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) $(HEADERS) -ggdb -Wall -o client

server : $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) $(HEADERS) -ggdb -Wall -o server
