AC_PREREQ([2.69])

AC_INIT([remote-dump1090], [0], [https://github.com/rafaelmartins/remote-dump1090])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.13 foreign dist-bzip2 dist-xz subdir-objects serial-tests])
AC_CONFIG_HEADERS([config.h])
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE([enable])

AC_PROG_CC

AC_CHECK_HEADERS([unistd.h signal.h stdarg.h syslog.h sys/types.h sys/socket.h arpa/inet.h netdb.h],
                 [], [unable to find some required header])

AC_CHECK_FUNCS([gethostbyname], [],
               [unable to find gethosbyname function in your libc])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AS_ECHO("
        ==== ${PACKAGE_STRING} ====

        prefix:       ${prefix}
        exec_prefix:  ${exec_prefix}
        bindir:       ${bindir}

        compiler:     ${CC}
        cflags:       ${CFLAGS}
        ldflags:      ${LDFLAGS}
")
