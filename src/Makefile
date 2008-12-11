CC	= gcc
CFLAGS	= -Wall -Wextra -O2 -ggdb
IFLAGS	= -g root -o root

OUT		= mumudvb
OBJ_FILES	= mumudvb.o tune.o udp.o dvb.o pat_rewrite.o cam.o cam_en50221.o log.o ts.o autoconf.o sap.o crc32.o
SRC_FILES	= $(OBJ_FILES:.o=.c)

ifdef DESTDIR
  BINDIR = $(DESTDIR)/usr/bin
else
  BINDIR = /usr/bin
endif

ifdef DEBUG
  CFLAGS += -g
endif

.PHONY: all clean clean_objs install uninstall

all: $(OUT)


mumudvb: $(OBJ_FILES)
ifdef LIBDVBEN50221
	$(CC) $(INCS) $(CFLAGS) -llibdvben50221 -I../lib/libdvben50221 -o $@ $^
else
	$(CC) $(INCS) $(CFLAGS) -o $@ $^
endif

clean: clean_objs
	-rm -f  $(OUT)

clean_objs:
	-rm -f  *.o *~ .depend

install: $(OUT)
	install $(IFLAGS) -d $(BINDIR)
	install $(IFLAGS) -m 755 mumudvb $(BINDIR)

uninstall:
	-rm $(BINDIR)/mumudvb
	-rm -r /var/run/mumudvb

%.o: %.c
ifdef LIBDVBEN50221
	$(CC) $(CFLAGS) -I../lib/libdvben50221 -c $<
else
	$(CC) $(CFLAGS) -c $<
endif
.depend: $(SRC_FILES)
	$(CC) -MM $^ > $@

include .depend
