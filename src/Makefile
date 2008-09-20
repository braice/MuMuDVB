CC	= gcc
CFLAGS	= -Wall -Wextra -O2
IFLAGS	= -g root -o root

OUT		= mumudvb
OBJ_FILES	= mumudvb.o tune.o udp.o dvb.o pat_rewrite.o cam.o cam_en50221.o log.o ts.o autoconf.o sap.o
SRC_FILES	= $(OBJ_FILES:.o=.c)

ifdef DESTDIR
  BINDIR = $(DESTDIR)/usr/bin
else
  BINDIR = /usr/bin
endif

ifdef DEBUG
  CFLAGS += -g
endif

ifdef UK
  CFLAGS += -DUK
endif

ifdef FINLAND
  CFLAGS += -DFINLAND
endif

ifdef FINLAND2
  CFLAGS += -DFINLAND2
endif

ifdef FRANCE
  CFLAGS += -DFRANCE
endif

.PHONY: all clean clean_objs install uninstall

all: $(OUT)

mumudvb: $(OBJ_FILES)
	$(CC) $(INCS) $(CFLAGS) -o $@ $^

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
	$(CC) $(CFLAGS) -c $<

.depend: $(SRC_FILES)
	$(CC) -MM $^ > $@

include .depend
