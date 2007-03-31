#ifndef GPSNAV_H
#define GPSNAV_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <sys/queue.h>

#define gps_error(format, args...) \
	fprintf(stderr, "ERROR: " format "\n" , ## args)

#define gps_debug(level, format, args...) \
	printf(format "\n" , ## args);

struct gpsnav;

struct gps_key_value {
	char *key;
	char *value;
};

struct gps_data_t;
struct gps_fix_t;
struct gps_map;
struct gps_map_provider;
struct gps_pixcache_entry;

struct gpsnav {
	struct gps_data_t *gps_conn;
	pthread_t gps_cb_handle;
	void (* update_cb)(void *, const struct gps_fix_t *);
	void *update_cb_data;

	LIST_HEAD(map_list, gps_map) map_list;
	LIST_HEAD(map_provider_list, gps_map_provider) map_prov_list;

	struct gps_pixcache_entry *pc_head, *pc_tail;
	unsigned int pc_max_size, pc_cur_size;
};

extern int gpsnav_init(struct gpsnav **gpsnav_out);
extern int gpsnav_connect(struct gpsnav *nav);
extern void gpsnav_disconnect(struct gpsnav *nav);
extern void gpsnav_finish(struct gpsnav *nav);
extern void gpsnav_set_update_callback(struct gpsnav *nav,
				       void (* update_cb)(void *, const struct gps_data_t *),
				       void *data);

extern struct gps_map_provider *gpsnav_find_provider(struct gpsnav *gpsnav, const char *name);

extern int gpsnav_mapdb_write(struct gpsnav *nav, const char *filename);
extern int gpsnav_mapdb_read(struct gpsnav *nav, const char *filename);

extern char *gpsnav_get_full_path(const char *fname, const char *base_path);
extern char *gpsnav_remove_base_path(const char *fname, const char *base_path);

#endif
