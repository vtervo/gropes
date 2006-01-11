#ifndef GPSNAV_PIXCACHE_H
#define GPSNAV_PIXCACHE_H

#include <sys/time.h>
#include <time.h>

#include <gpsnav/map.h>

struct gps_pixcache_entry {
	struct gps_map *map;
	struct gps_pixel_buf pb;
	unsigned int size;

	struct gps_pixcache_entry *next, *prev;
};

extern int gpsnav_pixcache_add(struct gpsnav *nav, struct gps_map *map,
			       struct gps_pixel_buf *pb);
extern int gpsnav_pixcache_get(struct gpsnav *nav, struct gps_map *map,
			       struct gps_pixel_buf *pb);
extern void gpsnav_pixcache_purge(struct gpsnav *nav);

#endif
