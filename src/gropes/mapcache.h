#include <gtk/gtk.h>
#include <gpsnav/gpsnav.h>
#include "gropes.h"

#ifndef GROPES_MAPCACHE_H
#define GROPES_MAPCACHE_H

struct gropes_mapcache_entry {
	GdkPixbuf *pb;
	unsigned int size;
	struct gps_map *map;
	uint16_t x;
	uint16_t y;
	double scale;

	struct gropes_mapcache_entry *next, *prev;
};

int gropes_mapcache_add(struct gropes_state *gs, struct gps_map *map,
			GdkPixbuf *pb, double scale);
GdkPixbuf *gropes_mapcache_get(struct gropes_state *gs, struct gps_map *map,
				struct gps_pixel_buf *pb, double scale);



#endif /* GROPES_MAPCACHE_H */
