CC	= gcc
CFLAGS	= -Wall -Wextra
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
  CFLAGS += -g -ggdb
else
  CFLAGS += -02
endif

ifdef GCOV
  CFLAGS += -fprofile-arcs -ftest-coverage 
endif

ifdef GPROF
  CFLAGS += -pg
endif


ifdef LIBDVBEN50221
  CFLAGS += -DLIBDVBEN50221
endif


.PHONY: all clean clean_objs install uninstall

all: $(OUT)

mumudvb: $(OBJ_FILES)
ifdef LIBDVBEN50221
	$(CC) $(INCS) $(CFLAGS) -ldvben50221 -ldvbapi -lucsi -lpthread -o $@ $^
else
	$(CC) $(INCS) $(CFLAGS) -o $@ $^
endif

clean: clean_objs
	-rm -f  $(OUT)

clean_objs:
	-rm -f  *.o *~ *.gcda *.gcno .depend

install: $(OUT)
	install $(IFLAGS) -d $(BINDIR)
	install $(IFLAGS) -m 755 mumudvb $(BINDIR)

uninstall:
	-rm $(BINDIR)/mumudvb
	-rm -r /var/run/mumudvb

%.o: %.c
ifdef LIBDVBEN50221
	$(CC) $(CFLAGS) -c $<
else
	$(CC) $(CFLAGS) -c $<
endif
.depend: $(SRC_FILES)
	$(CC) -MM $^ > $@

include .depend
