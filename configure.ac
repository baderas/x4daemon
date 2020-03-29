#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([X4Daemon], [0.4.5], [development@geekparadise.de], [x4daemon], [https://github.com/baderas/x4daemon])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([src/x4daemon.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_GREP
AC_PATH_PROG([MP], [modprobe], [no], [$PATH:/bin:/sbin:/usr/bin:/usr/sbin])
AC_DEFINE_UNQUOTED([_MP],["$MP"],[Path and check for modprobe])
if test "$MP" == no ; then
       AC_MSG_ERROR([modprobe is missing.])
fi
AC_PATH_PROG([LM], [lsmod], [0], [$PATH:/bin:/sbin:/usr/bin:/usr/sbin])
AC_DEFINE_UNQUOTED([_LM],["$LM"],[Path and check for lsmod])
if test "$_LM" == no ; then
       AC_MSG_ERROR([lsmod is missing.])
fi
AC_PATH_PROG([GREP], [grep], [0], [$PATH:/bin:/sbin:/usr/bin:/usr/sbin])
AC_DEFINE_UNQUOTED([_GREP],["$GREP"],[Path and check for grep])
if test "$_GREP" == no ; then
       AC_MSG_ERROR([grep is missing.])
fi

# Setting CFLAG
CFLAGS="$CFLAGS"

# Checks for libraries.
AC_CHECK_LIB([usb-1.0], [libusb_init],, [AC_MSG_ERROR([libusb-1.0 is missing.])],)

# Checks for header files.
AC_CHECK_HEADER([fcntl.h],,[AC_MSG_ERROR([fcntl.h is missing.])],)
AC_CHECK_HEADER([stdlib.h],,[AC_MSG_ERROR([stdlib.h is missing.])],)
AC_CHECK_HEADER([string.h],,[AC_MSG_ERROR([string.h is missing.])],)
AC_CHECK_HEADER([linux/input.h],,[AC_MSG_ERROR([input.h is missing.])],)
AC_CHECK_HEADER([linux/uinput.h],,[AC_MSG_ERROR([uinput.h is missing.])],)
AC_CHECK_HEADER([signal.h],,[AC_MSG_ERROR([signal.h is missing.])],)
AC_CHECK_HEADER([stdbool.h],,[AC_MSG_ERROR([stdbool.h is missing.])],)
AC_CHECK_HEADER([libusb-1.0/libusb.h],,[AC_MSG_ERROR([libusb-1.0/libusb.h is missing.])],)
AC_CHECK_HEADER([stdio.h],,[AC_MSG_ERROR([stdio.h is missing.])],)
AC_CHECK_HEADER([syslog.h],,[AC_MSG_ERROR([syslog.h is missing.])],)
AC_CHECK_HEADER([unistd.h],,[AC_MSG_ERROR([unistd.h is missing.])],)

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset malloc])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
