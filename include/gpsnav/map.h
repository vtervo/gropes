#ifndef GPSNAV_MAP_H
#define GPSNAV_MAP_H

#include <stdint.h>

#include <gpsnav/gpsnav.h>
#include <gpsnav/coord.h>

struct gps_pixel_buf {
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;
	int row_stride;
	uint8_t bpp;

	uint8_t *data;
	int can_free:1;
};

struct gps_map {
	struct gps_area area;
	struct gps_marea marea;
	int width, height;
	double scale_x, scale_y;
	void *proj;
	const struct gps_datum *datum; /* NULL means WGS-84 */
	struct gps_map_provider *prov;

	void *data;

	LIST_ENTRY(gps_map) entries;
};

struct gps_map_provider {
	const char *name;
	int (* get_pixels)(struct gpsnav *nav, struct gps_map *map,
			   struct gps_pixel_buf *pb);
	int (* add_map)(struct gpsnav *nav, struct gps_map *map,
			struct gps_key_value *kv, int kv_count,
			const char *base_path);
	int (* get_map_info)(struct gpsnav *nav, struct gps_map *map,
			     struct gps_key_value **kv, int *kv_count,
			     const char *base_path);
	void (* free_map)(struct gps_map *map);
	int (* init)(struct gpsnav *gpsnav, struct gps_map_provider *prov);
	void (* finish)(struct gpsnav *gpsnav, struct gps_map_provider *prov);
	void *data;

	LIST_ENTRY(gps_map_provider) entries;
};

extern int gpsnav_add_map(struct gpsnav *gpsnav, struct gps_map *map,
			  struct gps_key_value *prov_kv, int kv_count,
			  const char *base_path);

extern int gpsnav_get_map_pixels(struct gpsnav *gpsnav, struct gps_map *map,
				 struct gps_pixel_buf *pb);
extern int gpsnav_get_provider_map_info(struct gpsnav *nav, struct gps_map *map,
					struct gps_key_value **kv_out,
					int *kv_count, const char *base_path);
extern struct gps_map **gpsnav_find_maps(struct gpsnav *gpsnav,
					 int (* check_map)(struct gps_map *map, void *arg),
					 void *arg);
extern struct gps_map **gpsnav_find_maps_for_coord(struct gpsnav *gpsnav,
						   struct gps_coord *coord);
extern struct gps_map **gpsnav_find_maps_for_area(struct gpsnav *gpsnav,
						  const struct gps_area *area);
extern struct gps_map **gpsnav_find_maps_for_marea(struct gpsnav *gpsnav, struct gps_map *ref_map,
						   const struct gps_marea *area);

extern struct gps_map *gps_map_new(void);
extern void gps_map_free(struct gps_map *map);

#endif
