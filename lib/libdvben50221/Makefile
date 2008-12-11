# Makefile for linuxtv.org dvb-apps/lib/libdvben50221

includes = asn_1.h                 \
           en50221_app_ai.h        \
           en50221_app_auth.h      \
           en50221_app_ca.h        \
           en50221_app_datetime.h  \
           en50221_app_dvb.h       \
           en50221_app_epg.h       \
           en50221_app_lowspeed.h  \
           en50221_app_mmi.h       \
           en50221_app_rm.h        \
           en50221_app_smartcard.h \
           en50221_app_tags.h      \
           en50221_app_teletext.h  \
           en50221_app_utils.h     \
           en50221_errno.h         \
           en50221_session.h       \
           en50221_stdcam.h        \
           en50221_transport.h

objects  = asn_1.o                 \
           en50221_app_ai.o        \
           en50221_app_auth.o      \
           en50221_app_ca.o        \
           en50221_app_datetime.o  \
           en50221_app_dvb.o       \
           en50221_app_epg.o       \
           en50221_app_lowspeed.o  \
           en50221_app_mmi.o       \
           en50221_app_rm.o        \
           en50221_app_smartcard.o \
           en50221_app_teletext.o  \
           en50221_app_utils.o     \
           en50221_session.o       \
           en50221_stdcam.o        \
           en50221_stdcam_hlci.o   \
           en50221_stdcam_llci.o   \
           en50221_transport.o

lib_name = libdvben50221

CPPFLAGS += -I../../lib -DLOG_LEVEL=1 # FIXME

.PHONY: all

all: library

include ../../Make.rules
