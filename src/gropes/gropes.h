#ifndef GROPES_H
#define GROPES_H

#include <gtk/gtk.h>

#include <gpsnav/map.h>
#include <gpsnav/coord.h>
#include <gpsnav/gpsnav.h>
#include "ui.h"

#define GROPES_MODE_TERRESTRIAL	0
#define GROPES_MODE_NAUTICAL	1

struct map_state;

struct item_on_screen {
	struct gps_coord pos;
	struct gps_mcoord mpos;
	GdkRectangle area;
	double track;
	double speed;
	int pos_valid:1, on_screen:1, draw_trail:1;
	struct list *trail;
	void (* update_info)(struct map_state *, struct item_on_screen *);
};

struct map_on_screen {
	struct gps_map *map;
	GdkRectangle map_area;
	GdkRectangle draw_area;

	struct map_on_screen *next;
};

struct map_state {
	struct gps_coord center_pos;
	struct gps_mcoord center_mpos;
	struct gps_marea marea;
	struct gps_map *ref_map;
	double scale;
	struct map_on_screen *mos_list;
	int width, height;
	GtkWidget *darea;
	struct ui_info_area info_area;

	struct item_on_screen me;
	pthread_mutex_t mutex;
};

struct gropes_state {
	gpointer ui_data;
	GtkUIManager *ui_manager;

	struct gpsnav *nav;
	struct map_state big_map;
	int opt_follow_gps:1, opt_draw_map_rectangles:1;
	int mode;
};

char *fmt_coord(const struct gps_coord *coord, int fmt);
void fmt_mcoord(const struct gps_mcoord *coord, char *buf);
char *get_map_fname(struct gpsnav *nav, struct gps_map *map);
void redraw_map_area(struct gropes_state *state, struct map_state *ms,
		     GtkWidget *widget, const GdkRectangle *area);

void change_map_center(struct gropes_state *gs, struct map_state *map,
		       const struct gps_mcoord *cent, double scale);
void draw_maps(struct gropes_state *state, GtkWidget *widget,
	       struct map_on_screen *mos_list, const GdkRectangle *area);

void move_item(struct gropes_state *gs, struct map_state *ms,
	       struct item_on_screen *item, const struct gps_coord *pos);
void calc_item_pos(struct gropes_state *gs, struct map_state *ms,
		   struct item_on_screen *item);

int create_ui(struct gropes_state *gs);
int create_hildon_ui(struct gropes_state *gs);
int show_maptool_ui(struct gropes_state *gs);

void gropes_shutdown(void);

extern struct gropes_state gropes_state;

#endif
