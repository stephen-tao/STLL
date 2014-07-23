# make PREFIX=/some/where if you've got custom HarfBuzz installed somewhere

PREFIX?=/usr
export PKG_CONFIG_PATH:=$(PREFIX)/lib/pkgconfig
CC=g++
CXXFLAGS=-Wall -Wextra -pedantic -std=c++11 -g \
    `pkg-config sdl --cflags` \
    `pkg-config freetype2 --cflags` \
    `pkg-config harfbuzz --cflags`
LIBS=\
    `pkg-config sdl --libs` \
    `pkg-config freetype2 --libs` \
    `pkg-config harfbuzz --libs`

# so as not to have to use LD_LIBRARY_PATH when prefix is custom
LDFLAGS=-Wl,-rpath -Wl,$(PREFIX)/lib

all: test

test: main.o layouter.o layouterSDL.o layouterFont.o
	$(CC) -o test main.o layouter.o layouterSDL.o layouterFont.o $(LIBS)

clean:
	rm -f ex-sdl-freetype-harfbuzz
	rm *.o
	rm test