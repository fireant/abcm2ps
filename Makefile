# Makefile source for abcm2ps

VERSION = 7.6.8

CC = gcc
INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL}

CPPFLAGS = -DHAVE_CONFIG_H  -I.
CPPPANGO = 
CFLAGS = -g -O2 -Wall -pipe
LDFLAGS =  -lm

prefix = /usr/local
exec_prefix = ${prefix}

srcdir = .

bindir = ${exec_prefix}/bin
libdir = ${exec_prefix}/lib
datadir = ${prefix}/share
docdir = /usr/local/doc

# unix
OBJECTS=abc2ps.o \
	abcparse.o buffer.o deco.o draw.o format.o front.o glyph.o music.o parse.o \
	slre.o subs.o svg.o syms.o
abcm2ps: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(OBJECTS): abcparse.h config.h Makefile
abc2ps.o buffer.o deco.o draw.o format.o front.o glyph.o music.o parse.o \
	subs.o svg.o syms.o: abc2ps.h
abc2ps.o front.o: front.h
front.o parse.o slre.o: slre.h
subs.o: subs.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CPPPANGO) -c -o $@ $<

abcmfe: front.c front.h slre.h
	$(CC) $(CFLAGS) -DMAIN -o $@ $< slre.o

DOCFILES=$(addprefix $(srcdir)/,Changes License README *.abc *.eps *.txt)

install: abcm2ps
	mkdir -p $(bindir); \
	mkdir -p $(datadir)/abcm2ps; \
	mkdir -p $(docdir)/abcm2ps; \
	$(INSTALL_PROGRAM) abcm2ps $(bindir)
	for f in $(srcdir)/*.fmt; do \
		$(INSTALL_DATA) $$f $(datadir)/abcm2ps; \
	done
	for f in $(DOCFILES); do \
		$(INSTALL_DATA) $$f $(docdir)/abcm2ps; \
	done

uninstall:
	echo "uninstalling..."; \
	rm -f $(bindir)/abcm2ps; \
	rm -rf $(datadir)/abcm2ps; \
	rm -rf $(docdir)/abcm2ps

DIST_FILES = \
	abcm2ps-$(VERSION)/Changes \
	abcm2ps-$(VERSION)/INSTALL \
	abcm2ps-$(VERSION)/License \
	abcm2ps-$(VERSION)/Makefile \
	abcm2ps-$(VERSION)/Makefile.in \
	abcm2ps-$(VERSION)/README \
	abcm2ps-$(VERSION)/abc2ps.c \
	abcm2ps-$(VERSION)/abc2ps.h \
	abcm2ps-$(VERSION)/abcparse.c \
	abcm2ps-$(VERSION)/abcparse.h \
	abcm2ps-$(VERSION)/accordion.abc \
	abcm2ps-$(VERSION)/build.ninja \
	abcm2ps-$(VERSION)/buffer.c \
	abcm2ps-$(VERSION)/chinese.abc \
	abcm2ps-$(VERSION)/configure \
	abcm2ps-$(VERSION)/configure.in \
	abcm2ps-$(VERSION)/config.h \
	abcm2ps-$(VERSION)/config.h.in \
	abcm2ps-$(VERSION)/config.guess \
	abcm2ps-$(VERSION)/config.sub \
	abcm2ps-$(VERSION)/deco.c \
	abcm2ps-$(VERSION)/deco.abc \
	abcm2ps-$(VERSION)/draw.c \
	abcm2ps-$(VERSION)/features.txt \
	abcm2ps-$(VERSION)/flute.fmt \
	abcm2ps-$(VERSION)/format.c \
	abcm2ps-$(VERSION)/format.txt \
	abcm2ps-$(VERSION)/front.c \
	abcm2ps-$(VERSION)/front.h \
	abcm2ps-$(VERSION)/glyph.c \
	abcm2ps-$(VERSION)/install.sh \
	abcm2ps-$(VERSION)/landscape.fmt \
	abcm2ps-$(VERSION)/music.c \
	abcm2ps-$(VERSION)/musicfont.fmt \
	abcm2ps-$(VERSION)/newfeatures.abc \
	abcm2ps-$(VERSION)/options.txt \
	abcm2ps-$(VERSION)/parse.c \
	abcm2ps-$(VERSION)/sample.abc \
	abcm2ps-$(VERSION)/sample2.abc \
	abcm2ps-$(VERSION)/sample3.abc \
	abcm2ps-$(VERSION)/sample3.eps \
	abcm2ps-$(VERSION)/sample4.abc \
	abcm2ps-$(VERSION)/sample5.abc \
	abcm2ps-$(VERSION)/slre.c \
	abcm2ps-$(VERSION)/slre.h \
	abcm2ps-$(VERSION)/subs.c \
	abcm2ps-$(VERSION)/svg.c \
	abcm2ps-$(VERSION)/syms.c \
	abcm2ps-$(VERSION)/tight.fmt \
	abcm2ps-$(VERSION)/voices.abc

dist:
	ln -s . abcm2ps-$(VERSION); \
	tar -zcvf abcm2ps-$(VERSION).tar.gz $(DIST_FILES); \
	rm abcm2ps-$(VERSION)

zip-dist:
	ln -s . abcm2ps-$(VERSION); \
	zip -r abcm2ps-$(VERSION).zip $(DIST_FILES); \
	rm abcm2ps-$(VERSION)

zip: abcm2ps.exe
	strip abcm2ps.exe; \
	cd ..; zip -r abcm2ps-$(VERSION).zip \
	abcm2ps-$(VERSION)/abcm2ps.exe \
	abcm2ps-$(VERSION)/License \
	abcm2ps-$(VERSION)/Changes \
	abcm2ps-$(VERSION)/INSTALL \
	abcm2ps-$(VERSION)/sample3.eps \
	abcm2ps-$(VERSION)/*.abc \
	abcm2ps-$(VERSION)/*.fmt \
	abcm2ps-$(VERSION)/*.txt ; cd -

EXAMPLES = accordion.ps \
	chinese.ps \
	deco.ps \
	newfeatures.ps \
	sample.ps \
	sample2.ps \
	sample3.ps \
	sample4.ps \
	sample5.ps \
	voices.ps

test:	$(EXAMPLES)
%.ps: %.abc
	./abcm2ps -O $@ $<

clean:
	rm -f *.o $(EXAMPLES) # *.obj
