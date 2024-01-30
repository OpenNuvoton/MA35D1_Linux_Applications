.SUFFIXES : .x .o .c .s

CROSS_COMPILE = aarch64-nuvoton-linux-gnu-

CC = $(CROSS_COMPILE)gcc
STRIP = $(CROSS_COMPILE)strip

TARGET = ma35d1_bl2_offset
SRCS := ma35d1_bl2_offset.c
LIBS = -lpthread -lc -lgcc

all:
	$(CC) -static $(SRCS) -o $(TARGET) $(LIBS)
	$(STRIP) $(TARGET)

clean:
	rm -f *.o
	rm -f *.x
	rm -f *.flat
	rm -f *.map
	rm -f temp
	rm -f *.img
	rm -f $(TARGET)
	rm -f *.gdb
