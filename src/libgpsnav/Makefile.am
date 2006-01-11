## Makefile.am -- Process this file with automake to produce Makefile.in

MAINTAINERCLEANFILES = Makefile.in

SUBDIRS = proj4

EXTRA_DIST = gpsnav-config.in

INCLUDES = -Iproj4 $(LIBGPSNAV_INCLUDES) $(XML_CFLAGS) $(PNG_CFLAGS)

bin_SCRIPTS = gpsnav-config

lib_LTLIBRARIES		= libgpsnav.la
libgpsnav_la_SOURCES	= datum.c gpsnav.c map.c mapdb.c \
			  pixcache.c map-mericd.c map-raster.c
libgpsnav_la_LDFLAGS	= -version-info 0:1:0
libgpsnav_la_LIBADD	= proj4/libproj.la $(XML_LIBS) $(PNG_LIBS) \
			  $(LIBJPEG) $(LIBGPS) $(LIBGIF)
