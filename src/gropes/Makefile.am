# Process this file with automake to create Makefile.in

MAINTAINERCLEANFILES = Makefile.in
INCLUDES = @LIBGPSNAV_INCLUDES@ @LIBPROJ_INCLUDES@

bin_PROGRAMS = gropes gropes-maptool

gropes_CFLAGS = $(GTK_CFLAGS) $(PANGO_CFLAGS) $(GTHREAD_CFLAGS)
gropes_SOURCES = gropes.c ui.c map.c item.c
gropes_LDADD = @LIBGPSNAV@ $(GTK_LIBS) $(PANGO_LIBS) $(GTHREAD_LIBS)

if USE_HILDON
gropes_CFLAGS += $(HILDON_CFLAGS)
gropes_SOURCES += ui_hildon.c
gropes_LDADD += $(HILDON_LIBS)
else
gropes_SOURCES += ui_gtk.c
endif

gropes_maptool_CFLAGSADD = $(CURL_CFLAGS)
gropes_maptool_SOURCES = gropes-maptool.c gropes-maptool-hel-kartta.c \
			 gropes-maptool-oikotie.c gropes-maptool-expedia.c
gropes_maptool_LDADD = @LIBGPSNAV@ $(CURL_LIBS)

noinst_HEADERS = gropes.h
