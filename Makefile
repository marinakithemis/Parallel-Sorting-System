CC = gcc
CFLAGS = -Wall -g -lm
INCLUDES = -Iinclude

mysort_src = src/mysort.c
sorting1_src = src/sorting1.c
sorting2_src = src/sorting2.c

mysort_obj = src/mysort.o
sorting1_obj = src/sorting1.o
sorting2_obj = src/sorting2.o

EXECUTABLES = mysort sorting1 sorting2

all: $(EXECUTABLES)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

mysort: $(mysort_obj)
	$(CC) $(CFLAGS) $^ -o $@

sorting1: $(sorting1_obj)
	$(CC) $(CFLAGS) $^ -o $@

sorting2: $(sorting2_obj)
	$(CC) $(CFLAGS) $^ -o $@

valgrind:
	valgrind ./mysort -i testing_files/voters100000.bin -k 6 -e1 ./sorting1 -e2 ./sorting2

clean:
	rm -f $(mysort_obj) $(sorting1_obj) $(sorting2_obj) $(EXECUTABLES)


