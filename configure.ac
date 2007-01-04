dnl -*- mode: m4; -*-
dnl Process this file with autoconf to produce a configure script.

# Require autoconf 2.52
AC_PREREQ(2.52)

AC_INIT(src/libgpsnav/gpsnav.c)
AM_INIT_AUTOMAKE(gpsnav, 0.1.0)
AM_CONFIG_HEADER(config.h)

AC_CONFIG_MACRO_DIR([m4])

dnl Allow user to specify flags
AC_ARG_WITH(cflags,
	[  --with-cflags           Specify additional flags to pass to compiler],
	[
		if test "x$withval" != "xno" ; then
			CFLAGS="$CFLAGS $withval"
		fi
	]
)
AC_ARG_WITH(cppflags,
	[  --with-cppflags         Specify additional flags to pass to preprocessor] ,
	[
		if test "x$withval" != "xno"; then
			CPPFLAGS="$CPPFLAGS $withval"
		fi
	]
)
AC_ARG_WITH(ldflags,
	[  --with-ldflags          Specify additional flags to pass to linker],
	[
		if test "x$withval" != "xno" ; then
			LDFLAGS="$LDFLAGS $withval"
		fi
	]
)
AC_ARG_WITH(libs,
	[  --with-libs             Specify additional libraries to link with],
	[
		if test "x$withval" != "xno" ; then
			LIBS="$LIBS $withval"
		fi
	]
)


AC_ARG_WITH(common-dir,
	[  --with-common-dir=PATH  Specify path for common installation libraries],
	[
		if test "x$withval" != "xno" ; then
			trycommondir=$withval
		fi
	]
)

saved_LDFLAGS="$LDFLAGS"
saved_CPPFLAGS="$CPPFLAGS"
AC_CACHE_CHECK([for common directory], ac_cv_commondir, [
	for commondir in $trycommondir "" /usr/local /usr/pkg /opt ; do
		# Skip directories if they don't exist
		if test ! -z "$commondir" -a ! -d "$commondir" ; then
			continue;
		fi
		CPPFLAGS="$saved_CPPFLAGS"
		LDFLAGS="$saved_LDFLAGS"

		if test ! -z "$commondir" -a "x$commondir" != "x/usr"; then
			# Try to use $commondir/lib if it exists, otherwise
			# $commondir
			if test -d "$commondir/lib" ; then
				LDFLAGS="-L$commondir/lib $saved_LDFLAGS"
				if test ! -z "$need_dash_r" ; then
					LDFLAGS="-R$commondir/lib $LDFLAGS"
				fi
			else
				LDFLAGS="-L$commondir $saved_LDFLAGS"
				if test ! -z "$need_dash_r" ; then
					LDFLAGS="-R$commondir $LDFLAGS"
				fi
			fi
			# Try to use $commondir/include if it exists, otherwise
			# $commondir
			if test -d "$commondir/include" ; then
				CPPFLAGS="-I$commondir/include $saved_CPPFLAGS"
			else
				CPPFLAGS="-I$commondir $saved_CPPFLAGS"
			fi
		fi
		break;
	done

	if test -z "$commondir" ; then
		commondir="(system)"
	fi

	ac_cv_commondir=$commondir
])

if (test ! -z "$ac_cv_commondir" && test "x$ac_cv_commondir" != "x(system)") ; then
	dnl Need to recover commondir - test above runs in subshell
	commondir=$ac_cv_commondir
	if test ! -z "$commondir" -a "x$commondir" != "x/usr"; then
		# Try to use $commondir/lib if it exists, otherwise
		# $commondir
		if test -d "$commondir/lib" ; then
			LDFLAGS="-L$commondir/lib $saved_LDFLAGS"
			if test ! -z "$need_dash_r" ; then
				LDFLAGS="-R$commondir/lib $LDFLAGS"
			fi
		else
			LDFLAGS="-L$commondir $saved_LDFLAGS"
			if test ! -z "$need_dash_r" ; then
				LDFLAGS="-R$commondir $LDFLAGS"
			fi
		fi
		# Try to use $commondir/include if it exists, otherwise
		# $commondir
		if test -d "$commondir/include" ; then
			CPPFLAGS="-I$commondir/include $saved_CPPFLAGS"
		else
			CPPFLAGS="-I$commondir $saved_CPPFLAGS"
		fi
	fi
fi

dnl Checks for programs.
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PATH_PROG(TEST_MINUS_S_SH, bash)
AC_PATH_PROG(TEST_MINUS_S_SH, sh)

dnl Add libtool support.
AM_PROG_LIBTOOL
dnl Automatically update the libtool script if it becomes out-of-date.
#AC_SUBST(LIBTOOL_DEPS)

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([errno.h fcntl.h malloc.h stdlib.h string.h sys/time.h unistd.h getopt.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME

dnl Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([gettimeofday memset strdup strerror])

dnl C Compiler features
AC_C_INLINE
if test "$GCC" = "yes"; then
	CFLAGS="-Wall $CFLAGS"
fi

dnl Check for packages
PKG_CHECK_MODULES(GLIB, gobject-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(PANGO, pango)
AC_SUBST(PANGO_CFLAGS)
AC_SUBST(PANGO_LIBS)

PKG_CHECK_MODULES(GTHREAD, gthread-2.0)
AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)

PKG_CHECK_MODULES(GTK, gtk+-2.0)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

PKG_CHECK_MODULES(XML, libxml-2.0)
AC_SUBST(XML_CFLAGS)
AC_SUBST(XML_LIBS)

PKG_CHECK_MODULES(PNG, libpng)
AC_SUBST(PNG_CFLAGS)
AC_SUBST(PNG_LIBS)

dnl Test for libjpeg
if test -z "$LIBJPEG"; then
	AC_CHECK_LIB(jpeg, jpeg_destroy_decompress, jpeg_ok=yes, AC_MSG_ERROR(*** libjpeg not found ***))
	AC_CHECK_HEADER(jpeglib.h, , AC_MSG_ERROR(*** JPEG lib header file not found ***))
	LIBJPEG="-ljpeg"
fi
AC_SUBST(LIBJPEG)

dnl Test for libgif
if test -z "$LIBGIF"; then
	AC_CHECK_LIB(gif, DGifOpenFileName, gif_ok=yes, AC_MSG_ERROR(*** libgif not found ***))
	AC_CHECK_HEADER(gif_lib.h, , AC_MSG_ERROR(*** GIF lib header file not found ***))
	LIBGIF="-lgif"
fi
AC_SUBST(LIBGIF)

dnl Test for libgps
if test -z "$LIBGPS"; then
	AC_CHECK_LIB(gps, gps_open, , AC_MSG_ERROR(*** libgps not found ***))
	AC_CHECK_HEADER(gps.h, , AC_MSG_ERROR(*** gps.h not found ***))
	LIBGPS="-lgps"
fi
AC_SUBST(LIBGPS)

dnl Test for libcurl
LIBCURL_CHECK_CONFIG([yes],,[have_libcurl=yes])

dnl Enable/disable debugging messages.
AC_ARG_ENABLE(debug,
[  --enable-debug          enable debug messages. [default=no]],
	, enable_debug=no)
if (test x$enable_debug = xyes); then
	AC_DEFINE(DEBUG, 1, [Enable debug messages.])
fi

AC_SUBST(LIBGPSNAV)
LIBGPSNAV="\${top_srcdir}/src/libgpsnav/libgpsnav.la"

AC_SUBST(LIBGPSNAV_INCLUDES)
LIBGPSNAV_INCLUDES="-I\${top_srcdir}/include"

AC_SUBST(LIBPROJ_INCLUDES)
LIBPROJ_INCLUDES="-I\${top_srcdir}/src/libgpsnav/proj4"

# Pass information about installed gpsnav requirements, so that
# a proper gpsnav-config file will be build.
GPSNAV_LIBS="-L${libdir} -lgpsnav"
GPSNAV_CFLAGS="-I${includedir}"
AC_SUBST(GPSNAV_LIBS)
AC_SUBST(GPSNAV_CFLAGS)

pkgdata=`eval echo ${datadir}/${PACKAGE}` ; pkgdata=`eval echo ${pkgdata}`
AC_SUBST(pkgdata)

AC_OUTPUT([
Makefile
m4/Makefile
src/Makefile
src/libgpsnav/Makefile
src/libgpsnav/gpsnav-config
src/libgpsnav/proj4/Makefile
src/gropes/Makefile
include/Makefile
include/gpsnav/Makefile
])
