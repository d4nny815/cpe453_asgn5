CC = gcc
CFLAGS = -Wall -Werror -g -std=c99

.PHONY = all clean

all: min_common minls minget

minls: minls.o min_common.o
	$(CC) $(CFLAGS) $^ -o $@ 

minls.o: minls.c
	$(CC) $(CFLAGS) -c $< -o $@

minget: minget.o min_common.o
	$(CC) $(CFLAGS) $^ -o $@ 

minget.o: minget.c
	$(CC) $(CFLAGS) -c $< -o $@

min_common: min_common.o
	@echo "min_common built"

min_common.o: min_common.c min_common.h
	$(CC) $(CFLAGS) -c $< -o $@


clean: 
	rm -rf *.o minls minget