HDR = term.h mbf.h gif.h
SRC = ${HDR:.h=.c}
EHDR = default.h cs_vtg.h cs_437.h
ESRC = main.c

all: congif
congif: $(HDR) $(EHDR) $(SRC) $(ESRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(ESRC)
clean:
	$(RM) congif
