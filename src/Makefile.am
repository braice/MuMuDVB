# what flags you want to pass to the C compiler & linker
AM_CFLAGS = -Wall -Wextra
AM_LDFLAGS =

bin_PROGRAMS = mumudvb
mumudvb_SOURCES = autoconf.c crc32.c dvb.h log.c mumudvb.h network.h pat_rewrite.h \
		  rtp.h sap.h ts.h tune.h unicast_http.h autoconf.h dvb.c errors.h \
		  mumudvb.c network.c pat_rewrite.c rtp.c sap.c ts.c tune.c unicast_http.c

if BUILD_CAMSUPPORT
mumudvb_SOURCES += $(SOURCES_camsupport)
endif

SOURCES_camsupport = \
        cam.c \
	cam.h \
        $(NULL)
