AMIGA_PREFIX ?= /opt/amiga
NETINC ?= /opt/amiga-netinclude/include

CC = $(AMIGA_PREFIX)/bin/m68k-amigaos-gcc
CPPFLAGS = -Iinclude -I$(NETINC)
CFLAGS = -O2 -Wall -Wextra -mcrt=nix13

TARGET = build/MiniFTPD
OBJS = src/main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(OBJS)
	cp miniftpd.conf build/miniftpd.conf

clean:
	rm -f $(OBJS) $(TARGET) build/miniftpd.conf

.PHONY: all clean
