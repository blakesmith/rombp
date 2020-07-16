ifeq ($(TARGET),rg350)
	TOOLCHAIN=/home/blake/src/RG350_buildroot/output/host
	SYSROOT=$(TOOLCHAIN)/usr/mipsel-gcw0-linux-uclibc/sysroot
	CC=$(TOOLCHAIN)/usr/bin/mipsel-linux-gcc
else
	SYSROOT=/
	CC=gcc
endif

CFLAGS=-Wall -Isrc
LDFLAGS=-lSDL2 -lSDL2_ttf -lm -lstdc++ -pthread -Wl,--as-needed -Wl,--gc-sections -s

OPK_DIR=opk
OPK_FILE=$(PROG).gcw0.desktop
OPK_ICON=icon.png

C_SOURCES=src/rombp.c src/ips.c

OBJS=$(subst .c,.o,$(C_SOURCES))

PROG=rombp

all: $(PROG)

$(OPK_DIR):
	mkdir $(OPK_DIR)

$(PROG).opk: $(PROG) $(OPK_DIR)
	cp $(PROG) $(OPK_DIR)
	cp $(OPK_FILE) $(OPK_DIR)
	cp $(OPK_ICON) $(OPK_DIR)
	mksquashfs $(OPK_DIR) $@ -all-root -noappend -no-exports -no-xattrs

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) --sysroot=$(SYSROOT) -o $(PROG) $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) --sysroot=$(SYSROOT) -o $@ $<

clean:
	rm -rf $(PROG)
	rm -rf $(PROG).opk
	rm -rf $(OPK_DIR)
	rm -rf src/*.o

.PHONY: all clean
