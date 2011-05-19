#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([MuMuDVB], [1.6.1b_20110519_master], [mumudvb@braice.net])
AC_CONFIG_SRCDIR([src/mumudvb.c])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([src/config.h])

dnl
dnl Check for linux dvb api headers
dnl
AC_CHECK_HEADER([linux/dvb/dmx.h])
if test "$ac_cv_header_linux_dvb_dmx_h" = no; then
        AC_MSG_ERROR([linux dvb api headers are required to build])
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_CC_C99

dnl
dnl Iconv stuff
dnl
AM_ICONV


AC_CHECK_LIB([dvbapi], [dvbdemux_set_section_filter])
AC_CHECK_LIB([ucsi], [atsc_text_segment_decode],[atsc_long_names="yes"], [atsc_long_names="no"])
#AC_CHECK_LIB([dvben50221], [en50221_stdcam_create],,,[ucsi,pthread])
AC_CHECK_LIB([pthread], [pthread_create])


dnl
dnl cam support
dnl
AC_ARG_ENABLE(cam_support,
  [  --enable-cam-support          CAM support (default enabled)],,[enable_cam_support="yes"])

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
dnl transcoding support
dnl
AC_ARG_ENABLE(transcoding,
  [  --enable-transcoding          transcoding support (default disabled)],,[enable_transcoding="no"])

if test "${enable_transcoding}" = "yes"
then
  AC_CHECK_LIB([avformat], [url_split],[AC_DEFINE(HAVE_URL_SPLIT, 1, Define if the function url_split exists)], [])
  AC_CHECK_LIB([avformat], [ff_url_split],[AC_DEFINE(HAVE_FF_URL_SPLIT, 1, Define if the function ff_url_split exists)], [])
  AC_CHECK_HEADERS([libavcodec/avcodec.h ffmpeg/avcodec.h])
  AC_CHECK_HEADERS([libavformat/avformat.h ffmpeg/avformat.h])
  AC_CHECK_HEADERS([libswscale/swscale.h ffmpeg/swscale.h])
  AC_CHECK_LIB([avcodec], [av_picture_copy],[], [enable_transcoding="no"])
  AC_CHECK_LIB([avformat], [av_get_frame_filename],[], [enable_transcoding="no"])
  AC_CHECK_LIB([swscale], [sws_freeContext],[], [enable_transcoding="no"])
  if test "${enable_transcoding}" != "no"
  then
    AC_DEFINE(ENABLE_TRANSCODING, 1, Define if you want the transcoding support)
  else
    AC_MSG_WARN([libavcodev, libavformat and libswscale are needed for transcoding support])
  fi
else
  enable_transcoding="no"
fi
AM_CONDITIONAL(BUILD_TRANSCODING, [test "${enable_transcoding}" != "no"])


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
  [  --enable-duma          Debbuging DUMA library (default disabled)],,
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
AC_FUNC_MALLOC
AC_FUNC_REALLOC
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

if test "${enable_cam_support}" != "no" ; then
        echo "Build with CAM support:                             yes"
else
        echo "Build with CAM support:                              no"
fi


if test "${enable_transcoding}" != "no" ; then
        echo "Build with transcoding support:                     yes"
else
        echo "Build with transcoding support:                      no"
fi

if test "${atsc_long_names}" = "yes" ; then
        echo "Build with ATSC long names support:                 yes"
else
        echo "Build with ATSC long names support:                  no"
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

