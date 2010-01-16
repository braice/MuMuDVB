/* src/config.h.  Generated from config.h.in by configure.  */
/* src/config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if you want the CAM support */
/* #undef ENABLE_CAM_SUPPORT */

/* Define if you want the transcoding support */
/* #undef ENABLE_TRANSCODING */

/* Define to 1 if you have the `alarm' function. */
#define HAVE_ALARM 1

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if you have the <arpa/nameser.h> header file. */
#define HAVE_ARPA_NAMESER_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the <ffmpeg/avcodec.h> header file. */
/* #undef HAVE_FFMPEG_AVCODEC_H */

/* Define to 1 if you have the <ffmpeg/avformat.h> header file. */
/* #undef HAVE_FFMPEG_AVFORMAT_H */

/* Define to 1 if you have the <ffmpeg/swscale.h> header file. */
/* #undef HAVE_FFMPEG_SWSCALE_H */

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the iconv() function and it works. */
#define HAVE_ICONV 1

/* Define to 1 if you have the `inet_ntoa' function. */
#define HAVE_INET_NTOA 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `avcodec' library (-lavcodec). */
/* #undef HAVE_LIBAVCODEC */

/* Define to 1 if you have the <libavcodec/avcodec.h> header file. */
/* #undef HAVE_LIBAVCODEC_AVCODEC_H */

/* Define to 1 if you have the `avformat' library (-lavformat). */
/* #undef HAVE_LIBAVFORMAT */

/* Define to 1 if you have the <libavformat/avformat.h> header file. */
/* #undef HAVE_LIBAVFORMAT_AVFORMAT_H */

/* Define to 1 if you have the `duma' library (-lduma). */
/* #undef HAVE_LIBDUMA */

/* Define to 1 if you have the `dvbapi' library (-ldvbapi). */
/* #undef HAVE_LIBDVBAPI */

/* Define to 1 if you have the `dvben50221' library (-ldvben50221). */
/* #undef HAVE_LIBDVBEN50221 */

/* Define to 1 if you have the `pthread' library (-lpthread). */
#define HAVE_LIBPTHREAD 1

/* Define to 1 if you have the `swscale' library (-lswscale). */
/* #undef HAVE_LIBSWSCALE */

/* Define to 1 if you have the <libswscale/swscale.h> header file. */
/* #undef HAVE_LIBSWSCALE_SWSCALE_H */

/* Define to 1 if you have the `ucsi' library (-lucsi). */
/* #undef HAVE_LIBUCSI */

/* Define to 1 if your system has a GNU libc compatible `malloc' function, and
   to 0 otherwise. */
#define HAVE_MALLOC 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if your system has a GNU libc compatible `realloc' function,
   and to 0 otherwise. */
#define HAVE_REALLOC 1

/* Define to 1 if you have the <resolv.h> header file. */
#define HAVE_RESOLV_H 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <values.h> header file. */
#define HAVE_VALUES_H 1

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST 

/* Name of package */
#define PACKAGE "mumudvb"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "mumudvb@braice.net"

/* Define to the full name of this package. */
#define PACKAGE_NAME "MuMuDVB"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "MuMuDVB 1.6.1b_20100101"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "mumudvb"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.6.1b_20100101"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "1.6.1b_20100101"

/* Define for Solaris 2.5.1 so the uint32_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT32_T */

/* Define for Solaris 2.5.1 so the uint8_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT8_T */

/* Define to the type of a signed integer type of width exactly 32 bits if
   such a type exists and the standard includes do not define it. */
/* #undef int32_t */

/* Define to rpl_malloc if the replacement function should be used. */
/* #undef malloc */

/* Define to rpl_realloc if the replacement function should be used. */
/* #undef realloc */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to the type of an unsigned integer type of width exactly 16 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint16_t */

/* Define to the type of an unsigned integer type of width exactly 32 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint32_t */

/* Define to the type of an unsigned integer type of width exactly 8 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint8_t */
