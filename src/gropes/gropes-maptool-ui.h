#include <gtk/gtk.h>
#include <gpsnav/gpsnav.h>
#include <gpsnav/map.h>
#include <gpsnav/coord.h>

#ifndef GROPES_MAPTOOL_UI_H
#define GROPES_MAPTOOL_UI_H

enum {
	OPT_ADD_MERICD_MAPS = 0x100,
	OPT_DL_KARTTA_HEL_MAPS,
	OPT_DL_OIKOTIE_MAPS,
	OPT_DL_EXPEDIA_MAPS,
	OPT_DL_KARTTAPAIKKA_MAPS
};

struct maptool_state;

struct maptool_provider {
	const char *text;
	void (* create_ui)(struct maptool_state *);
	void (* destroy_ui)(struct maptool_state *);
};

struct maptool_state {
	GtkWidget *main_win;
	struct gpsnav *nav;
	gchar *mapdb_file;
	GtkWidget *map_box;
	const struct maptool_provider *active_prov;
};

extern int kartta_hel_dl(struct gpsnav *nav, struct gps_coord *start,
			 struct gps_coord *end, const char *map);
extern int oikotie_dl(struct gpsnav *nav, struct gps_coord *start,
		      struct gps_coord *end, const char *map);
extern int expedia_dl(struct gpsnav *nav, struct gps_coord *start,
		      struct gps_coord *end, const char *scale);
extern int karttapaikka_dl(struct gpsnav *nav, struct gps_coord *start,
		      struct gps_coord *end, const char *scale);

#endif
