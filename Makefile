.POSIX:

PREFIX = /usr/local

# FreeBSD ships clang as cc, and X11 comes from ports under /usr/local, which
# is not on the compiler's default search path.
CC = cc
X11INC = /usr/local/include
X11LIB = /usr/local/lib

dwmblocks: dwmblocks.o
	$(CC) dwmblocks.o -L$(X11LIB) -lX11 -o dwmblocks
dwmblocks.o: dwmblocks.c config.h
	$(CC) -c -I$(X11INC) dwmblocks.c
clean:
	rm -f *.o *.gch dwmblocks
install: dwmblocks
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f dwmblocks $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/dwmblocks
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/dwmblocks

.PHONY: clean install uninstall
