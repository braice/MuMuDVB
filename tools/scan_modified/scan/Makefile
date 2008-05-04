
CC = gcc
CFLAGS = -MD -g -Wall -O2 -I./include
LFLAGS = -g -Wall

OBJS = diseqc.o dump-mumudvb.o dump-zap.o dump-vdr.o scan.o lnb.o section.o atsc_psip_section.o
SRCS = $(OBJS:.o=.c)

TARGET = scan

$(TARGET): $(OBJS)
	$(CC) $(LFLAGS) -o $(TARGET) $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) *.o *.d $(TARGET)

-include $(wildcard *.d) dummy

