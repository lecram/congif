PREFIX=/usr/local
MANPREFIX=$(PREFIX)/man
BINDIR=$(DESTDIR)$(PREFIX)/bin
MANDIR=$(DESTDIR)$(MANPREFIX)/man1
DEFAULT_FONT=misc-fixed-6x10.mbf

HDR = term.h mbf.h gif.h
SRC = ${HDR:.h=.c}
EHDR = default.h cs_vtg.h cs_437.h default_font.h
ESRC = main.c

all: congif

congif: $(HDR) $(EHDR) $(SRC) $(ESRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(ESRC)

default_font.h: $(DEFAULT_FONT) mbf.h mbf.c mbf2c.c
	$(CC) $(CFLAGS) -o mbf2c mbf.c mbf2c.c
	./mbf2c $(DEFAULT_FONT) > fnt.tmp
	mv fnt.tmp $@

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
	$(RM) congif default_font.h mbf2c fnt.tmp
