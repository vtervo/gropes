# Process this file with automake to create Makefile.in

MAINTAINERCLEANFILES = Makefile.in
INCLUDES = @LIBGPSNAV_INCLUDES@ @LIBPROJ_INCLUDES@

bin_PROGRAMS = gropes gropes-maptool

gropes_CFLAGS = $(GTK_CFLAGS) $(PANGO_CFLAGS) $(GTHREAD_CFLAGS) $(HILDON_CFLAGS)
gropes_SOURCES = gropes.c ui.c map.c item.c ui_gtk.c ui_hildon.c mapcache.c
gropes_LDADD = @LIBGPSNAV@ $(GTK_LIBS) $(PANGO_LIBS) $(GTHREAD_LIBS) $(HILDON_LIBS)

gropes_maptool_CFLAGSADD = $(CURL_CFLAGS)
gropes_maptool_SOURCES = gropes-maptool.c gropes-maptool-hel-kartta.c \
			 gropes-maptool-oikotie.c gropes-maptool-expedia.c
gropes_maptool_LDADD = @LIBGPSNAV@ $(CURL_LIBS)

noinst_HEADERS = gropes.h
