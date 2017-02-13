PREFIX=/usr/local
MANPREFIX=$(PREFIX)/man
BINDIR=$(DESTDIR)$(PREFIX)/bin
MANDIR=$(DESTDIR)$(MANPREFIX)/man1

HDR = term.h mbf.h gif.h
SRC = ${HDR:.h=.c}
EHDR = default.h cs_vtg.h cs_437.h
ESRC = main.c

all: congif

congif: $(HDR) $(EHDR) $(SRC) $(ESRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(ESRC)

install: congif
	rm -f $(BINDIR)/congif
	mkdir -p $(BINDIR)
	cp congif $(BINDIR)/congif
	mkdir -p $(MANDIR)
	cp congif.1 $(MANDIR)/congif.1

uninstall: $(BINDIR)/congif
	rm $(BINDIR)/congif
	rm $(MANDIR)/congif.1
clean:
	$(RM) congif
