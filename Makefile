CC=g++
CFLAGS=-Wall -Wpedantic -ansi -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 `xml2-config --cflags` -g -ggdb
LDFLAGS=-Wall -Wpedantic -ansi -lpcre -lfuse `xml2-config --libs` -g -ggdb
srcdir=src
builddir=build

all: $(builddir) loggedfs

$(builddir):
	mkdir $(builddir)

loggedfs: $(builddir)/loggedfs.o $(builddir)/FSOperations.o $(builddir)/Format.o $(builddir)/Config.o $(builddir)/Filter.o
	$(CC) -o loggedfs $(builddir)/loggedfs.o $(builddir)/FSOperations.o $(builddir)/Format.o $(builddir)/Config.o $(builddir)/Filter.o $(LDFLAGS)

$(builddir)/loggedfs.o:
	$(CC) -o $(builddir)/loggedfs.o -c $(srcdir)/loggedfs.cpp $(CFLAGS)

$(builddir)/FSOperations.o:
	$(CC) -o $(builddir)/FSOperations.o -c $(srcdir)/FSOperations.cpp $(CFLAGS)

$(builddir)/Format.o:
	$(CC) -o $(builddir)/Format.o -c $(srcdir)/Format.cpp $(CFLAGS)

$(builddir)/Config.o:
	$(CC) -o $(builddir)/Config.o -c $(srcdir)/Config.cpp $(CFLAGS)

$(builddir)/Filter.o:
	$(CC) -o $(builddir)/Filter.o -c $(srcdir)/Filter.cpp $(CFLAGS)

clean:
	rm -rf $(builddir)/
	
install:
	gzip loggedfs.1
	cp loggedfs.1.gz /usr/share/man/man1/
	cp loggedfs /usr/bin/
	cp loggedfs.xml /etc/


mrproper: clean
	rm -rf loggedfs
			
release:
	tar -c --exclude="CVS" $(srcdir)/ loggedfs.xml LICENSE loggedfs.1.gz Makefile | bzip2 - > loggedfs.tar.bz2

