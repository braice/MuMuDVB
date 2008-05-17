CC	= gcc
CFLAGS	= -Wall -O2
IFLAGS	= -g root -o root

SRC_FILES	= $(wildcard *.c)
OUT		= $(SRC_FILES:.c=)

ifdef DESTDIR
  BINDIR = $(DESTDIR)/usr/bin
else
  BINDIR = /usr/bin
endif

ifdef DEBUG
  CFLAGS += -g
endif

.PHONY: all clean clean_objs 

all: $(OUT)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean: clean_objs
	-rm -f $(OUT)

clean_objs:
	-rm -f *.o *~ .depend

#install: $(OUT)
#	install $(IFLAGS) -d $(BINDIR)
#	install $(IFLAGS) -m 755 $(OUT) $(BINDIR)

#uninstall:
#	-rm $(foreach file,$(OUT),$(BINDIR)/$(file))

%.o: %.c
	$(CC) $(CFLAGS) -c $<

.depend: $(SRC_FILES)
	$(CC) -MM $^ > $@

include .depend
