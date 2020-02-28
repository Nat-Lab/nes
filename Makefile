CFLAGS=-g -Wall -Wextra
TARGETS=nes
OBJS=6502.o gfx.o main.o mem.o ppu.o rom.o sdl.o

.PHONY: all clean
all: $(TARGETS)

nes: $(OBJS)
	$(CC) -o nes $(OBJS) $(CFLAGS) -lsdl2

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(TARGETS) *.o 
