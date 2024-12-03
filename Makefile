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

# Generate the documentation PDF to be printed
# Required packages: fd, xargs, enscript, ghostscript, pandoc, texlive-medium, qpdf
doc :
	# Convert README to pdf
	pandoc README.md -s -V papersize:a4 --pdf-engine=lualatex -o readme.pdf
	# Convert code to highlighted pdf
	fd --regex ".*\.(c|h)" | xargs enscript --color=1 -C -Ec -o - | ps2pdf - code.pdf
	# Combine cover, readme and code into one pdf
	qpdf code.pdf --pages cover.pdf 1 readme.pdf 1 code.pdf 1-z -- documentation.pdf
