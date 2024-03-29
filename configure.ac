#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([MuMuDVB], [m4_join([_], [2.1.0], m4_chomp_all(m4_esyscmd([git rev-parse HEAD])))], [mumudvb@braice.net])
AC_CONFIG_SRCDIR([src/mumudvb.c])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([src/config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

dnl
dnl Iconv stuff
dnl
AM_ICONV

dnl
dnl dvb-api support (disable on request or check existence of header)
dnl
AC_ARG_ENABLE(dvb_support,
  [  --enable-dvb-support    linux dvb-api support (default enabled)],,[enable_dvb_support="yes"])
if test "${enable_dvb_support}" = "yes"
then
  AC_CHECK_HEADER([linux/dvb/dmx.h])
  if test "$ac_cv_header_linux_dvb_dmx_h" = no; then
    AC_MSG_WARN([linux dvb api headers not found, disabling dvb api functionality])
    AC_DEFINE(DISABLE_DVB_API, 1, Define if you want to bulid without DVB API)
  fi
else
  enable_dvb_support="no"
  AC_DEFINE(DISABLE_DVB_API, 1, Define if you want to bulid without DVB API)
fi

AC_CHECK_LIB([dvbapi], [dvbdemux_set_section_filter])
AC_CHECK_LIB([ucsi], [atsc_text_segment_decode],[atsc_long_names="yes"], [atsc_long_names="no"])
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([rt], [clock_gettime])
AC_CHECK_LIB([m], [log2l])


dnl
dnl cam support
dnl
AC_ARG_ENABLE(cam_support,
  [  --enable-cam-support    CAM support (default enabled)],,[enable_cam_support="yes"])

if test "${enable_cam_support}" = "yes"
then
  AC_CHECK_LIB([ucsi], [atsc_text_segment_decode],[], [enable_cam_support="no"])
  AC_CHECK_LIB([dvben50221], [en50221_tl_create],[], [enable_cam_support="no"])
  if test "${enable_cam_support}" != "no"
  then
    AC_DEFINE(ENABLE_CAM_SUPPORT, 1, Define if you want the CAM support)
  else
    AC_MSG_WARN([libucsi and libdvben50221 are needed for CAM support])
  fi
else
  enable_cam_support="no"
fi
AM_CONDITIONAL(BUILD_CAMSUPPORT, [test "${enable_cam_support}" != "no"])

dnl
dnl arib support
dnl
AC_ARG_ENABLE(arib_support,
  [  --enable-arib-support   ARIB charset support (default enabled)],,[enable_arib_support="yes"])

if test "${enable_arib_support}" = "yes"
then
  AC_CHECK_HEADER([aribb24/aribb24.h], [], [enable_arib_support="no"])
  if test "${enable_arib_support}" != "no"
  then
    AC_CHECK_LIB([aribb24], [arib_instance_new], [], [enable_arib_support="no"])
  fi
  if test "${enable_arib_support}" != "no"
  then
    AC_DEFINE(ENABLE_ARIB_SUPPORT, 1, Define if you want the ARIB charset support)
  else
    AC_MSG_WARN([libaribb24 is needed for ARIB charset support])
  fi
else
  enable_arib_support="no"
fi
AM_CONDITIONAL(BUILD_ARIB_SUPPORT, [test "${enable_arib_support}" != "no"])

dnl
dnl scam support
dnl
AC_ARG_ENABLE(scam_support,
  [  --enable-scam-support   SCAM support (default enabled)],,[enable_scam_support="yes"])

if test "${enable_scam_support}" = "yes"
then
  AC_CHECK_LIB([dvbcsa], [dvbcsa_bs_decrypt],[], [enable_scam_support="no"])
  if test "${enable_scam_support}" != "no"
  then
    AC_DEFINE(ENABLE_SCAM_SUPPORT, 1, Define if you want the SCAM support)
  else
    AC_MSG_WARN([libdvbcsa are needed for SCAM support])
  fi
else
  enable_scam_support="no"
fi
AM_CONDITIONAL(BUILD_SCAMSUPPORT, [test "${enable_scam_support}" != "no"])

dnl
dnl  Test coverage
dnl
AC_ARG_ENABLE(coverage,
	[  --enable-coverage       build for test coverage (default disabled)],,
	[enable_coverage="no"])
AS_IF([test "${enable_coverage}" != "no"], [
	CFLAGS="-fprofile-arcs -ftest-coverage ${CFLAGS}"
	CXXFLAGS="-fprofile-arcs -ftest-coverage ${CXXFLAGS}"
	LDFLAGS="-lgcov ${LDFLAGS}"
])

dnl
dnl duma support
dnl
AC_ARG_ENABLE(duma,
  [  --enable-duma           Debbuging DUMA library (default disabled)],,
  [enable_duma="no"])

if test "${enable_duma}" = "yes"
then
  AC_CHECK_LIB([duma], [_duma_malloc],[], [enable_duma="no"])
  if test "${enable_duma}" != "no"
  then
    LDFLAGS="-lduma ${LDFLAGS}"
  else
    AC_MSG_WARN([The DUMA library needs to be installed before])
  fi
fi

AC_ARG_ENABLE(android,
  [  --enable-android        Build for android (default disabled)],,[enable_android="no"])

if test "${enable_android}" = "yes"
then
  AC_DEFINE(ANDROID, 1, Define if you want build for android)
fi

AC_ARG_ENABLE(tune_old,
  [  --enable-tune_old       Use old code for tuning (default disabled)],,[enable_tune_old="no"])

if test "${enable_tune_old}" = "yes"
then
  AC_DEFINE(TUNE_OLD, 1, Define if you want the old code for tuning)
fi

# Checks for header files.
AC_HEADER_RESOLV
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h unistd.h values.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([alarm gettimeofday inet_ntoa memset socket strerror strstr])

AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 src/Makefile])
AC_OUTPUT

#put resume at the end
echo ""
echo "-------------------------------------------------------"
echo "MuMuDVB configure results:"
echo ""
echo "Features"
echo ""

if test "${enable_dvb_support}" != "no" ; then
        echo "Build with Linux DVB-API support:                   yes"
else
        echo "Build with Linux DVB-API support:                    no"
fi

if test "${enable_cam_support}" != "no" ; then
        echo "Build with CAM support:                             yes"
else
        echo "Build with CAM support:                              no"
fi

if test "${enable_scam_support}" != "no" ; then
        echo "Build with SCAM support:                            yes"
else
        echo "Build with SCAM support:                             no"
fi

if test "${atsc_long_names}" = "yes" ; then
        echo "Build with ATSC long names support:                 yes"
else
        echo "Build with ATSC long names support:                  no"
fi

if test "${enable_arib_support}" != "no" ; then
        echo "Build with ARIB charset support:                    yes"
else
        echo "Build with ARIB charset support:                     no"
fi

if test "${enable_android}" = "yes" ; then
        echo "Build with compatibility for android:               yes"
else
        echo "Build with compatibility for android:                no"
fi

if test "${enable_tune_old}" = "yes" ; then
        echo "Build with old tuning code:                         yes"
fi

echo ""
echo "Debugging"
echo ""

if test "${enable_coverage}" = "yes" ; then
        echo "Build with code coverage debug symbols              yes"
else
        echo "Build with code coverage debug symbols               no"
fi

if test "${enable_duma}" = "yes" ; then
        echo "Build with debugging DUMA library                    yes"
else
        echo "Build with debugging DUMA library                    no"
fi
