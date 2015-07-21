all: congif
congif: term.c mbf.c gif.c main.c
	$(CC) $(CFLAGS) -o $@ term.c mbf.c gif.c main.c
