.SUFFIXES : .x .o .c .s

CC := aarch64-linux-gnu-gcc
STRIP := aarch64-linux-gnu-strip

TARGET = rpmsg_demo_v2
SRCS := rpmsg_v2.c
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
