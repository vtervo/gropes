#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <endian.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gpsnav/gpsnav.h>
#include <gpsnav/map.h>
#include <gpsnav/coord.h>

#include <gps.h>

#include "gropes.h"

struct gropes_state gropes_state;

char *fmt_coord(const struct gps_coord *coord, int fmt)
{
	char buf[64];
	int deg_la, deg_lo, min_la, min_lo;
	double sec_la, sec_lo;
	char ns, ew;

	ns = 'N';
	ew = 'E';
	deg_la = floor(coord->la);
	deg_lo = floor(coord->lo);
	min_la = floor((coord->la - deg_la) * 60.0);
	min_lo = floor((coord->lo - deg_lo) * 60.0);
	sec_la = (coord->la - deg_la - min_la / 60.0) * 3600.0;
	sec_lo = (coord->lo - deg_lo - min_lo / 60.0) * 3600.0;
	switch (fmt) {
	case 0:
		sprintf(buf, "%f°%c, %f°%c", coord->la, ns, coord->lo, ew);
		break;
	case 1:
		sprintf(buf, "%d°%02d.%03d'%c, %d°%02d.%03d'%c",
			deg_la, min_la, (int) (sec_la * 1000 / 60), ns,
			deg_lo, min_lo, (int) (sec_lo * 1000 / 60), ew);
		break;
	case 2:
		sprintf(buf, "%d°%02d'%02d\" %c, %d°%02d'%02d\" %c",
			deg_la, min_la, (int) sec_la, ns,
			deg_lo, min_lo, (int) sec_lo, ew);
		break;
	}
	return strdup(buf);
}

void fmt_mcoord(const struct gps_mcoord *coord, char *buf)
{
	sprintf(buf, "%.1f, %.1f", coord->n, coord->e);
}

char *get_map_fname(struct gpsnav *nav, struct gps_map *map)
{
	struct gps_key_value *kv;
	int kv_count, i;
	char *fname;

	gpsnav_get_provider_map_info(nav, map, &kv, &kv_count, NULL);
	fname = kv[0].value;
	for (i = 0; i < kv_count; i++) {
		free(kv[i].key);
		if (i != 0)
			free(kv[i].value);
	}
	free(kv);
	return fname;
}

static GdkPixbuf *pixbuf_rotate(GdkPixbuf *pixbuf, double angle)
{
	int x, y, x_rot, y_rot, x_tran, y_tran, center_x, center_y;
	int channels, channels_rot, rowstride, rowstride_rot;
	int width, height;
	double cosv, sinv;
	guchar *pixels, *pixels_rot, *p, *p_rot;
	GdkPixbuf *pixbuf_rot;

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	pixbuf_rot = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);

	pixels = gdk_pixbuf_get_pixels(pixbuf);
	pixels_rot = gdk_pixbuf_get_pixels(pixbuf_rot);
	channels = gdk_pixbuf_get_n_channels(pixbuf);
	channels_rot = gdk_pixbuf_get_n_channels(pixbuf_rot);
	rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	rowstride_rot = gdk_pixbuf_get_rowstride(pixbuf_rot);
	cosv = cos(2 * M_PI - angle);
	sinv = sin(2 * M_PI - angle);

	center_x = width / 2;
	center_y = height / 2;

	memset(pixels_rot, 0x00, width * height * channels);
	for (y_rot = 0; y_rot < height; y_rot++) {
		for (x_rot = 0; x_rot < width; x_rot++) {
			x_tran = x_rot - center_x;
			y_tran = y_rot - center_y;

			x = x_tran * cosv - y_tran * sinv;
			y = x_tran * sinv + y_tran * cosv;

			x += center_x;
			y += center_y;

			if (x >= 0 && y >= 0 && x < width && y < height) {
				p = pixels + y * rowstride + x * channels;
				p_rot = pixels_rot + y_rot * rowstride_rot + x_rot * channels_rot;
				memcpy(p_rot, p, channels);
			}
		}
	}

	return pixbuf_rot;
}

static void draw_vehicle(struct gpsnav *nav, struct map_state *state)
{
	GdkGC *gc;
	GdkColor color;
	GdkRectangle *area;

	gc = gdk_gc_new(GDK_DRAWABLE(state->darea->window));
	if (gc == NULL)
		return;
	color.red = 0;
	color.green = 0;
	color.blue = 0xffff;
	color.pixel = 0x0000ff;
	area = &state->me.area;
	gdk_gc_set_foreground(gc, &color);
	printf("vehicle %d, %d (%d, %d)\n", area->x, area->y,
	       area->width, area->height);
	gdk_draw_arc(state->darea->window, gc, TRUE, area->x, area->y,
		     area->width, area->height, 0, 360*64);
	g_object_unref(gc);
}

void redraw_map_area(struct gropes_state *state, struct map_state *ms,
		     GtkWidget *widget, const GdkRectangle *area)
{
	GdkColor blue, black;
	GdkRectangle tmp;

	printf("Redraw request for %d:%d->%d:%d\n", area->x, area->y,
	       area->x + area->width, area->y + area->height);
	blue.red = 0;
	blue.green = 0;
	blue.blue = 0xffff;
	blue.pixel = 0x0000ff;
	black.red = 0;
	black.green = 0;
	black.blue = 0;
	black.pixel = 0;
	draw_maps(state, widget, ms->mos_list, area);
	if (ms->me.pos_valid && ms->me.on_screen &&
	    gdk_rectangle_intersect((GdkRectangle *) area, &ms->me.area, &tmp))
		draw_vehicle(state->nav, ms);
#if 0
	if (1) {
		PangoLayout *pl;
		PangoAttrList *attr_list;
		PangoAttribute *size;

		attr_list = pango_attr_list_new();
		size = pango_attr_size_new(21 * PANGO_SCALE);
		size->start_index = 0;
		size->end_index = G_MAXINT;
		pango_attr_list_insert(attr_list, size);
		pl = gtk_widget_create_pango_layout(widget, NULL);
		pango_layout_set_attributes(pl, attr_list);
		pango_layout_set_text(pl, "testing", 7);
		gdk_draw_layout_with_colors(widget->window,
					    widget->style->fg_gc[GTK_STATE_NORMAL],
					    0, 0, pl, &black, NULL);
		pango_attr_list_unref(attr_list);
		g_object_unref(pl);

		attr_list = pango_attr_list_new();
		size = pango_attr_size_new(20 * PANGO_SCALE);
		size->start_index = 0;
		size->end_index = G_MAXINT;
		pango_attr_list_insert(attr_list, size);
		pl = gtk_widget_create_pango_layout(widget, NULL);
		pango_layout_set_attributes(pl, attr_list);
		pango_layout_set_text(pl, "testing", 7);
		gdk_draw_layout_with_colors(widget->window,
					    widget->style->fg_gc[GTK_STATE_NORMAL],
					    1, 1, pl, &blue, NULL);
		pango_attr_list_unref(attr_list);
		g_object_unref(pl);
	}
#endif
}

#if 0
static gboolean on_win_key_pressed(GtkWidget *widget,
				   GdkEventKey *event,
				   gpointer user_data)
{
	struct gropes_state *state = user_data;
	struct map_state *ms = &state->big_map;

	switch (event->keyval) {
	case GDK_minus:
		printf("Zooming out\n");
		change_map_center(state->nav, ms, &ms->center_mpos, ms->scale * 1.5);
		break;
	case GDK_Up:
		scroll_map(state, ms, 0, -ms->height / 4, ms->scale);
		break;
	case GDK_Down:
		scroll_map(state, ms, 0, ms->height / 4, ms->scale);
		break;
	case GDK_Left:
		scroll_map(state, ms, -ms->width / 4, 0, ms->scale);
		break;
	case GDK_Right:
		scroll_map(state, ms, ms->width / 4, 0, ms->scale);
		break;
	case GDK_c:
		printf("Connecting\n");
		if (gpsnav_connect(state->nav) == 0)
			printf("Connected\n");
		break;
	case GDK_d:
		printf("Disconnecting\n");
		gpsnav_disconnect(state->nav);
		printf("Disconnected\n");
		break;
	case GDK_r:
		state->opt_draw_map_rectangles = !state->opt_draw_map_rectangles;
		printf("%s map rectangles\n", state->opt_draw_map_rectangles ?
		       "Drawing" : "Not drawing");
		gtk_widget_queue_draw_area(ms->darea, 0, 0,
					   ms->darea->allocation.width,
					   ms->darea->allocation.height);
		break;
		
	}
	return TRUE;
}
#endif


void gropes_shutdown(void)
{
	gpsnav_finish(gropes_state.nav);
	gtk_main_quit();
}

static void on_main_window_destroy(GtkWidget *widget, gpointer data)
{
	gropes_shutdown();
}

static void update_cb(void *arg, const struct gps_fix_t *fix)
{
	struct gropes_state *gs = arg;
	struct map_state *ms = &gs->big_map;
	struct item_on_screen *item;

	item = &ms->me;
	if (fix->mode == MODE_NOT_SEEN) {
		printf("No GPS fix\n");
		if (!item->pos_valid)
			return;
		item->pos_valid = item->on_screen = 0;
		gdk_threads_enter();
		move_item(gs, ms, item, NULL);
		gdk_threads_leave();
	} else {
		struct gps_coord pos;

		pos.la = fix->latitude;
		pos.lo = fix->longitude;
		gdk_threads_enter();
		move_item(gs, ms, item, &pos);
		gdk_threads_leave();
	}
}

int main(int argc, char *argv[])
{
	struct gpsnav *nav;
	struct gps_map *ref_map;
	struct gps_coord center_pos;
	struct gps_mcoord center_mpos;
	double scale;
	GtkWidget *window;
	struct gps_map **maps;
	int i, r;

	gtk_init(&argc, &argv);
	g_thread_init(NULL);
	gdk_threads_init();

	memset(&gropes_state, 0, sizeof(gropes_state));

	if (gpsnav_init(&nav) < 0)
		return 1;
	gropes_state.nav = nav;

	gpsnav_set_update_callback(nav, update_cb, &gropes_state);

	/* Pixel cache of 10 MB by default */
	nav->pc_max_size = 10 * 1024 * 1024;

	r = gpsnav_mapdb_read(nav, "mapdb.xml");
	if (r < 0)
		return 1;

	pthread_mutex_init(&gropes_state.big_map.mutex, NULL);

	scale = 0;
#if 1
	/* Munkkiniemen silta */
	center_pos.la = 60.195666667;
	center_pos.lo = 24.887666667;
#endif
#if 0
	/* Otaniemen lahdennyppylä */
	center_pos.la = 60 + 11.31 / 60.0;
	center_pos.lo = 24 + 49.34 / 60.0;
#endif
#if 0
	/* Ongelmallinen MeriCD-paikka */
	center_pos.la = 59.981282;
	center_pos.lo = 24.062860;
	scale = 9.602449;
#endif
	maps = gpsnav_find_maps_for_coord(nav, &center_pos);
	if (maps == NULL) {
		printf("No map found for coordinates\n");
		return -1;
	}

	/* Find the map with the largest scale */
	ref_map = maps[0];
	for (i = 1; maps[i] != NULL; i++) {
		if (maps[i]->scale_x > ref_map->scale_x)
			ref_map = maps[i];
	}
	if (scale == 0)
		scale = (ref_map->scale_x + ref_map->scale_y) / 2;

	gpsnav_get_metric_for_coord(ref_map, &center_pos, &center_mpos);

	free(maps);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "Gropes");

	create_ui(&gropes_state);

	gropes_state.big_map.me.area.height = gropes_state.big_map.me.area.width = 10;
	gropes_state.big_map.ref_map = ref_map;
	change_map_center(&gropes_state, &gropes_state.big_map, &center_mpos, scale);

	gtk_signal_connect(GTK_OBJECT(window), "destroy", GTK_SIGNAL_FUNC(on_main_window_destroy), NULL);

	gtk_widget_show_all(window);

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	return 0;
}
