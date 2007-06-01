#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>

#include <gps.h>
#include <gpsnav/gpsnav.h>
#include <gpsnav/map.h>
#include <gpsnav/coord.h>
#include <gpsnav/pixcache.h>

/* This is so fucking lame */
static struct gpsnav *lame_gpsnav_pointer;

static void gps_callback(struct gps_data_t *sentence, char *buf, size_t len,
			 int level)
{
	struct gpsnav *nav;

	nav = lame_gpsnav_pointer;

	if (nav->update_cb != NULL)
		nav->update_cb(nav->update_cb_data, sentence);
}

void gpsnav_set_update_callback(struct gpsnav *nav,
				void (* update_cb)(void *, const struct gps_data_t *),
				void *data)
{
	nav->update_cb = update_cb;
	nav->update_cb_data = data;
}

int gpsnav_connect(struct gpsnav *nav)
{
	/* Check if we're already connected */
	if (nav->gps_conn != NULL)
		return 0;
	nav->gps_conn = gps_open(NULL, NULL);
	if (nav->gps_conn == NULL) {
		gps_error("Unable to connect to gpsd");
		return -1;
	}
	lame_gpsnav_pointer = nav;
	if (gps_set_callback(nav->gps_conn, gps_callback,
			     &nav->gps_cb_handle) < 0) {
		gps_error("gps_set_callback failed()");
		return -1;
	}

	return 0;
}

void gpsnav_disconnect(struct gpsnav *nav)
{
	if (nav->gps_conn == NULL)
		return;
//	gps_del_callback(nav->gps_conn, &nav->gps_cb_handle);
	gps_close(nav->gps_conn);
	nav->gps_conn = NULL;
}

extern struct gps_map_provider mericd_provider;
extern struct gps_map_provider raster_provider;

static struct gps_map_provider *prov_table[] = {
	&raster_provider,
	&mericd_provider,
};

struct gps_map_provider *gpsnav_find_provider(struct gpsnav *nav, const char *name)
{
	struct gps_map_provider *prov;

	for (prov = nav->map_prov_list.lh_first; prov != NULL; prov = prov->entries.le_next) {
		if (strcmp(prov->name, name) == 0)
			return prov;
	}
	return NULL;
}

static void add_provider(struct gpsnav *nav, const struct gps_map_provider *prov)
{
	struct gps_map_provider *new_prov;

	new_prov = malloc(sizeof(*new_prov));
	if (new_prov == NULL) {
		gps_error("malloc failed");
		return;
	}
	memset(new_prov, 0, sizeof(*new_prov));
	*new_prov = *prov;

	if (new_prov->init(nav, new_prov) < 0) {
		gps_error("Map provider '%s' init failed", new_prov->name);
		free(new_prov);
		return;
	}
	LIST_INSERT_HEAD(&nav->map_prov_list, new_prov, entries);
}

int gpsnav_init(struct gpsnav **gpsnav_out)
{
	struct gpsnav *gpsnav;
	int i;

	gpsnav = malloc(sizeof(*gpsnav));
	if (gpsnav == NULL)
		return -1;
	memset(gpsnav, 0, sizeof(*gpsnav));
	LIST_INIT(&gpsnav->map_list);
	LIST_INIT(&gpsnav->map_prov_list);

	for (i = 0; i < sizeof(prov_table)/sizeof(prov_table[0]); i++)
		add_provider(gpsnav, prov_table[i]);

	*gpsnav_out = gpsnav;

	return 0;
}

void gpsnav_finish(struct gpsnav *nav)
{
	struct gps_map *map;
	struct gps_map_provider *prov;

	map = nav->map_list.lh_first;
	while (map != NULL) {
		struct gps_map *next;

		next = map->entries.le_next;
		gps_map_free(map);
		map = next;
	}
	prov = nav->map_prov_list.lh_first;
	while (prov != NULL) {
		struct gps_map_provider *next;

		next = prov->entries.le_next;
		if (prov->finish != NULL)
			prov->finish(nav, prov);
		free(prov);
		prov = next;
	}
	gpsnav_pixcache_purge(nav);
	if (nav->gps_conn != NULL)
		gps_close(nav->gps_conn);
	free(nav);
}

char *gpsnav_get_full_path(const char *fname, const char *base_path)
{
	char buf[512], buf2[512];
	int len;
	const char *p;

	len = strlen(fname);
	if (base_path != NULL)
		len += strlen(base_path);
	len++;
	if (len >= sizeof(buf))
		return NULL;

	if (fname[0] != '/' && base_path != NULL)
		sprintf(buf2, "%s/%s", base_path, fname);
	else
		strcpy(buf2, fname);
	p = strrchr(buf2, '/');
	if (p != NULL) {
		char oldcwd[512];

		strncpy(buf, buf2, p - buf2);
		buf[p - buf2] = '\0';
		getcwd(oldcwd, sizeof(oldcwd));
		if (chdir(buf) < 0) {
			gps_error("Unable to chdir to '%s'", buf);
			return NULL;
		}
		getcwd(buf, sizeof(buf));
		chdir(oldcwd);
		p++;
	} else {
		getcwd(buf, sizeof(buf));
		p = buf2;
	}
	if (strlen(buf) + strlen(p) + 1 >= sizeof(buf))
		return NULL;
	sprintf(buf + strlen(buf), "/%s", p);

	return strdup(buf);
}

char *gpsnav_remove_base_path(const char *fname, const char *base_path)
{
	if (base_path == NULL)
		return strdup(fname);
	if (strncmp(fname, base_path, strlen(base_path)) == 0)
		return strdup(fname + strlen(base_path) + 1);
	else
		return strdup(fname);
}
