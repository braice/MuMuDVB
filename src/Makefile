CC	= gcc
CFLAGS	= -Wall -Wextra
IFLAGS	= -g root -o root

OUT		= mumudvb
OBJ_FILES	= mumudvb.o tune.o udp.o dvb.o pat_rewrite.o cam.o log.o ts.o autoconf.o sap.o crc32.o
SRC_FILES	= $(OBJ_FILES:.o=.c)

ifdef DESTDIR
  BINDIR = $(DESTDIR)/usr/bin
else
  BINDIR = /usr/bin
endif

ifdef DEBUG
  CFLAGS += -g -ggdb
else
  CFLAGS += -O2
endif

ifdef GCOV
  CFLAGS += -fprofile-arcs -ftest-coverage
endif

ifdef GPROF
  CFLAGS += -pg
endif


#Test if we do not asked the libusci
ifndef LIBUCSI
  CFLAGS += -DLIBUCSI
  LFLAGS += -lucsi
else
ifneq ($(LIBUCSI),0)
  CFLAGS += -DLIBUCSI
  LFLAGS += -lucsi
else
  NOCAMSUPPORT=1
endif
endif

ifndef NOCAMSUPPORT
  CFLAGS += -DLIBDVBEN50221
  LFLAGS += -ldvben50221 -ldvbapi -lucsi -lpthread
endif


.PHONY: all clean clean_objs install uninstall

all: $(OUT)

mumudvb: $(OBJ_FILES)
	$(CC) $(INCS) $(CFLAGS) $(LFLAGS) -o $@ $^

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
	$(CC) $(CFLAGS) -c $<

.depend: $(SRC_FILES)
	$(CC) -MM $^ > $@

include .depend
