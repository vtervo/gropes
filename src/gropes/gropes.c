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
#include <jpeglib.h>
#include <gpsnav/gpsnav.h>
#include <gpsnav/map.h>
#include <gpsnav/coord.h>

#include <gps.h>

#define DEFAULT_WIDTH		800
#define DEFAULT_HEIGHT		480

#define GROPES_MODE_TERRESTRIAL	0
#define GROPES_MODE_NAUTICAL	1

struct item_on_screen {
	struct gps_coord pos;
	struct gps_mcoord mpos;
	GdkRectangle area;
	double track;
	int pos_valid:1, on_screen:1;
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

	struct item_on_screen me;
	pthread_mutex_t mutex;
};

struct gropes_state {
	GtkUIManager *ui_manager;

	struct gpsnav *nav;
	struct map_state big_map;
	int opt_follow_gps:1, opt_draw_map_rectangles:1;
	int mode;
} gropes_state;

static char *fmt_coord(const struct gps_coord *coord, int fmt)
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

static void fmt_mcoord(const struct gps_mcoord *coord, char *buf)
{
	sprintf(buf, "%.1f, %.1f", coord->n, coord->e);
}

static char *get_map_fname(struct gpsnav *nav, struct gps_map *map)
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

static struct gps_map *find_best_map(struct gps_map *ref_map, struct gps_map **maps,
				     const struct gps_marea *area, double scale)
{
	struct gps_map *best_map = NULL;
	double best_points = 0;
	int i;

	for (i = 0; maps[i] != NULL; i++) {
		struct gps_marea isect;
		double points, scale_factor;
		struct gps_map *map;

		map = maps[i];
		gpsnav_calc_metric_isect(&map->marea, area, &isect);
		/* Check if map has at least 1 pixel of screen area in
		 * both directions */
		if (isect.end.n < isect.start.n + 1.0 * scale ||
		    isect.end.e < isect.start.e + 1.0 * scale)
			continue;
		/* Check if map has at least 1 pixel of map area in
		 * both directions */
		if (isect.end.n < isect.start.n + 1.0 * map->scale_y ||
		    isect.end.e < isect.start.e + 1.0 * map->scale_x)
			continue;

		points = (isect.end.n - isect.start.n) * (isect.end.e - isect.start.e);

		if (map->scale_y > scale)
			scale_factor = scale / map->scale_y;
		else {
			scale_factor = map->scale_y / scale;
			/* Do not allow zooming out too much */
			if (scale_factor < 0.25)
			    scale_factor = 0;
		}
		/* A good scale is vewy, vewy important for us */
		scale_factor /= 2;

		points *= scale_factor;
		if (points > best_points) {
			best_map = map;
			best_points = points;
		}
	}
	return best_map;
}

static void calc_xy_for_metric(struct gps_map *map, const GdkRectangle *screen_area,
			       double scale, const struct gps_marea *smarea,
			       struct gps_marea *isect, GdkRectangle *map_area,
			       GdkRectangle *map_draw_area)
{
	double ms_x, ms_y, me_x, me_y; /* map start and end */
	double ss_x, ss_y, se_x, se_y; /* screen start and end */

	ms_y = map->height - (isect->end.n - map->marea.start.n) / map->scale_y;
	me_y = map->height - (isect->start.n - map->marea.start.n) / map->scale_y;
	ms_x = (isect->start.e - map->marea.start.e) / map->scale_x;
	me_x = (isect->end.e - map->marea.start.e) / map->scale_x;
        /* Convert to width and height */
	me_x -= ms_x;
	me_y -= ms_y;

	ss_y = screen_area->height - (isect->end.n - smarea->start.n) / scale;
	se_y = screen_area->height - (isect->start.n - smarea->start.n) / scale;
	ss_x = (isect->start.e - smarea->start.e) / scale;
	se_x = (isect->end.e - smarea->start.e) / scale;
	/* Convert to width and height */
	se_x -= ss_x;
	se_y -= ss_y;
	/* Make relative to the supplied screen area */
	ss_y += screen_area->y;
	ss_x += screen_area->x;

//	printf("map    %f, %f --> %f, %f\n", ms_x, ms_y, me_x, me_y);
//	printf("screen %f, %f --> %f, %f\n", ss_x, ss_y, se_x, se_y);

	ss_x = rint(ss_x);
	ss_y = rint(ss_y);
	ms_x = rint(ms_x);
	ms_y = rint(ms_y);
	/* Let's check if we can do without scaling by rounding
	 * appropriately */
	if (fabs(me_x - se_x) < 1.0) {
		double avg = (me_x + se_x) / 2;

		if (ss_x + ceil(avg) <= screen_area->x +screen_area->width &&
		    ms_x + ceil(avg) <= map->width)
			avg = ceil(avg);
		else
			avg = floor(avg);
		me_x = avg;
		se_x = avg;
	} else {
		me_x = rint(me_x);
		se_x = rint(se_x);
	}
	if (fabs(me_y - se_y) < 1.0) {
		double avg = (me_y + se_y) / 2;

		if (ss_y + ceil(avg) <= screen_area->y + screen_area->height &&
		    ms_y + ceil(avg) <= map->height)
			avg = ceil(avg);
		else
			avg = floor(avg);
		me_y = avg;
		se_y = avg;
	} else {
		me_y = rint(me_y);
		se_y = rint(se_y);
	}

//	printf("map    %f, %f --> %f, %f\n", ms_x, ms_y, me_x, me_y);
//	printf("screen %f, %f --> %f, %f\n", ss_x, ss_y, se_x, se_y);

	map_area->x = ms_x;
	map_area->y = ms_y;
	map_area->width = me_x;
	map_area->height = me_y;

	map_draw_area->x = ss_x;
	map_draw_area->y = ss_y;
	map_draw_area->width = se_x;
	map_draw_area->height = se_y;
}

static void grey_fill(GtkWidget *widget, const GdkRectangle *area)
{
	GdkGC *gc;

	gc = gdk_gc_new(GDK_DRAWABLE(widget->window));
	gdk_gc_copy(gc, widget->style->fg_gc[GTK_STATE_NORMAL]);
	gdk_gc_set_foreground(gc, &widget->style->bg[GTK_STATE_NORMAL]);
	gdk_draw_rectangle(GDK_DRAWABLE(widget->window), gc, TRUE, area->x, area->y,
			   area->width, area->height);
	g_object_unref(gc);

}

struct map_on_screen {
	struct gps_map *map;
	GdkRectangle map_area;
	GdkRectangle draw_area;

	struct map_on_screen *next;
};

static int add_mos_entry(struct map_on_screen **head, struct gps_map *map,
			 const GdkRectangle *map_area, const GdkRectangle *draw_area)
{
	struct map_on_screen *e;

	e = malloc(sizeof(*e));
	if (e == NULL)
		return -1;
	e->map = map;
	if (map_area != NULL)
		e->map_area = *map_area;
	e->draw_area = *draw_area;
	*head = e;
	e->next = NULL;

	return 0;
}

static void free_mos_list(struct map_on_screen *e)
{
	struct map_on_screen *next;

	while (e != NULL) {
		next = e->next;
		free(e);
		e = next;
	}
}

#define FILL_UP		(1 << 0)
#define FILL_DOWN	(1 << 1)
#define FILL_LEFT	(1 << 2)
#define FILL_RIGHT	(1 << 3)

/*  +-----------+
 *  |           |
 *  |  +-----+  |
 *  |  |  1  |  |
 *  |  +-----+  |
 *  |     2     |
 *  +-----------+
 */
static int compare_areas(const GdkRectangle *a1,
			 const GdkRectangle *a2)
{
	int res = 0;

	if (a1->x > a2->x)
		res |= FILL_LEFT;
	if (a1->y > a2->y)
		res |= FILL_UP;
	if (a1->x + a1->width < a2->x + a2->width)
		res |= FILL_RIGHT;
	if (a1->y + a1->height < a2->y + a2->height)
		res |= FILL_DOWN;
	return res;
}

static struct map_on_screen **get_next_head(struct map_on_screen **head)
{
	struct map_on_screen *e;

	e = *head;
	while (e != NULL) {
		head = &e->next;
		e = e->next;
	}
	return head;
}

static void print_area(const char *name, const struct gps_marea *ma,
		       const GdkRectangle *a)
{
	char ss[64], se[64];

	fmt_mcoord(&ma->start, ss);
	fmt_mcoord(&ma->end, se);
	printf("%s %s --> %s  %4d, %4d --> %4d, %4d\n",
	       name, ss, se, a->x, a->y, a->x + a->width, a->y + a->height);
}

static int get_xy_on_screen(struct map_state *ms, const struct gps_mcoord *mpos,
			    int *x_out, int *y_out)
{
	struct gps_marea *marea;
	int x, y;

	marea = &ms->marea;

	if (mpos->n < marea->start.n || mpos->n >= marea->end.n)
		return -1;
	if (mpos->e < marea->start.e || mpos->e >= marea->end.e)
		return -1;

	x = (mpos->e - marea->start.e) / ms->scale;
	y = (marea->end.n - mpos->n) / ms->scale;
	assert(x >= 0 && y >= 0);
	assert(x < ms->width && y < ms->height);

	*x_out = x;
	*y_out = y;

	return 0;
}

static int generate_map_layout(struct gpsnav *nav, struct gps_map *ref_map,
			       struct gps_map **maps,
			       const GdkRectangle *zero_area,
			       const struct gps_marea *zero_marea, double scale,
			       const GdkRectangle *screen_area,
			       struct map_on_screen **head, int depth)
{
	struct gps_map *map;
	struct gps_marea isect, marea;
	GdkRectangle map_area, map_draw_area;
	int r, comp_res;

	marea.end.n = zero_marea->end.n - screen_area->y * scale;
	marea.start.e = zero_marea->start.e + screen_area->x * scale;
	marea.start.n = marea.end.n - screen_area->height * scale;
	marea.end.e = marea.start.e + screen_area->width * scale;
	printf("\n");
	print_area("zero ", zero_marea, zero_area);
	print_area("gen  ", &marea, screen_area);
	assert(screen_area->width > 0);
	assert(screen_area->height > 0);
	assert(screen_area->x >= 0);
	assert(screen_area->y >= 0);
	assert(screen_area->x + screen_area->width <= zero_area->width);
	assert(screen_area->y + screen_area->height <= zero_area->height);
	assert(depth < 16);

	map = find_best_map(ref_map, maps, &marea, scale);
	if (map == NULL) {
		printf("no map found\n");
		/* Put an empty entry */
		return add_mos_entry(head, NULL, NULL, screen_area);
	}

	gpsnav_calc_metric_isect(&marea, &map->marea, &isect);

	calc_xy_for_metric(map, screen_area, scale, &marea, &isect, &map_area,
			   &map_draw_area);

	print_area("map  ", &map->marea, &map_area);
	print_area("isect", &isect, &map_draw_area);
	{
		char *fname = get_map_fname(nav, map);

		printf("%s\n", fname);
		free(fname);
	}

	assert(map_area.x >= 0 && map_area.y >= 0);
	assert(map_area.x + map_area.width <= map->width);
	assert(map_area.y + map_area.height <= map->height);
	assert(map_area.width > 0 && map_area.height > 0);

	assert(map_draw_area.x >= screen_area->x && map_draw_area.y >= screen_area->y);
	assert(map_draw_area.width > 0 && map_draw_area.height > 0);
	assert(map_draw_area.x + map_draw_area.width <= screen_area->x + screen_area->width);
	assert(map_draw_area.y + map_draw_area.height <= screen_area->y + screen_area->height);

	comp_res = compare_areas(&map_draw_area, screen_area);
	if (comp_res == 0) {
		/* Map fills the whole area. Yay! */
		return add_mos_entry(head, map, &map_area, screen_area);
	}
	printf("comp_res: %c%c%c%c\n",
	       comp_res & FILL_UP ? 'U' : ' ',
	       comp_res & FILL_DOWN ? 'D' : ' ',
	       comp_res & FILL_LEFT ? 'L' : ' ',
	       comp_res & FILL_RIGHT ? 'R' : ' ');

	r = add_mos_entry(head, map, &map_area, &map_draw_area);
	if (r)
		return r;

	head = &(*head)->next;
	depth++;
	if (comp_res & FILL_UP) {
		GdkRectangle new_area;

		new_area = *screen_area;
		new_area.height = map_draw_area.y - screen_area->y;
		printf("Drawing upper\n");
		r = generate_map_layout(nav, ref_map, maps, zero_area,
					zero_marea, scale, &new_area,
					head, depth);
		if (r)
			return r;
		head = get_next_head(head);
	}
	if (comp_res & FILL_DOWN) {
		GdkRectangle new_area;

		new_area.x = screen_area->x;
		new_area.width = screen_area->width;
		new_area.y = map_draw_area.y + map_draw_area.height;
		new_area.height = screen_area->y + screen_area->height - new_area.y;
		printf("Drawing lower\n");
		r = generate_map_layout(nav, ref_map, maps, zero_area,
					zero_marea, scale, &new_area,
					head, depth);
		if (r)
			return r;
		head = get_next_head(head);
	}
	if (comp_res & FILL_LEFT) {
		GdkRectangle new_area;

		new_area.x = screen_area->x;
		new_area.width = map_draw_area.x - screen_area->x;
		new_area.y = map_draw_area.y;
		new_area.height = map_draw_area.height;
		printf("Drawing left\n");
		r = generate_map_layout(nav, ref_map, maps, zero_area,
					zero_marea, scale, &new_area, head,
					depth);
		if (r)
			return r;
		head = get_next_head(head);
	}
	if (comp_res & FILL_RIGHT) {
		GdkRectangle new_area;

		new_area.x = map_draw_area.x + map_draw_area.width;
		new_area.width = screen_area->x + screen_area->width - new_area.x;
		new_area.y = map_draw_area.y;
		new_area.height = map_draw_area.height;
		printf("Drawing right\n");
		r = generate_map_layout(nav, ref_map, maps, zero_area,
					zero_marea, scale, &new_area, head,
					depth);
		if (r)
			return r;
		head = get_next_head(head);
	}

	return 0;
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

static void destroy_pix_buf(guchar *pixels, gpointer data)
{
	struct gps_pixel_buf *pb = data;

	if (pb->can_free)
		free(pb->data);
}

static void draw_single_map(GtkWidget *widget, struct gpsnav *nav,
			    struct map_on_screen *mos, const GdkRectangle *isect)
{
	struct gps_pixel_buf pb_need, pb_got;
	GdkPixbuf *map_pb;

	memset(&pb_need, 0, sizeof(pb_need));
	pb_need.x = mos->map_area.x + (isect->x - mos->draw_area.x);
	pb_need.y = mos->map_area.y + (isect->y - mos->draw_area.y);
	pb_need.width = isect->width;
	pb_need.height = isect->height;
	pb_need.bpp = 24;
	pb_need.row_stride = pb_need.width * 3;
	pb_got = pb_need;

	if (gpsnav_get_map_pixels(nav, mos->map, &pb_got) < 0) {
		fprintf(stderr, "gpsnav_get_map_pixels() failed\n");
		grey_fill(widget, isect);
		return;
	}

	map_pb = gdk_pixbuf_new_from_data(pb_got.data, GDK_COLORSPACE_RGB, FALSE, 8,
					  pb_got.width, pb_got.height, pb_got.row_stride,
					  destroy_pix_buf, &pb_got);
	if (map_pb == NULL) {
		grey_fill(widget, isect);
		return;
	}
	gdk_draw_pixbuf(widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
			map_pb, pb_need.x - pb_got.x, pb_need.y - pb_got.y,
			isect->x, isect->y, isect->width, isect->height,
			GDK_RGB_DITHER_NONE, 0, 0);
	g_object_unref(map_pb);
}

static void draw_single_map_scaled(GtkWidget *widget, struct gpsnav *nav,
				   struct map_on_screen *mos, GdkRectangle *isect)
{
	struct gps_pixel_buf pb;
	GdkPixbuf *map_pb, *sub_pb, *scaled_pb;

	memset(&pb, 0, sizeof(pb));
	pb.x = mos->map_area.x;
	pb.y = mos->map_area.y;
	pb.width = mos->map_area.width;
	pb.height = mos->map_area.height;
	pb.bpp = 24;
	pb.row_stride = pb.width * 3;

	if (gpsnav_get_map_pixels(nav, mos->map, &pb) < 0) {
		fprintf(stderr, "gpsnav_get_map_pixels() failed\n");
		goto fail;
	}

	map_pb = gdk_pixbuf_new_from_data(pb.data, GDK_COLORSPACE_RGB, FALSE, 8,
					  pb.width, pb.height, pb.row_stride,
					  destroy_pix_buf, &pb);
	if (map_pb == NULL)
		goto fail;
	sub_pb = gdk_pixbuf_new_subpixbuf(map_pb, mos->map_area.x - pb.x,
					  mos->map_area.y - pb.y,
					  mos->map_area.width,
					  mos->map_area.height);
	g_object_unref(map_pb);
	scaled_pb = gdk_pixbuf_scale_simple(sub_pb, mos->draw_area.width,
					    mos->draw_area.height,
					    GDK_INTERP_BILINEAR);
	g_object_unref(sub_pb);
	if (scaled_pb == NULL)
		goto fail;
	gdk_draw_pixbuf(widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
			scaled_pb, isect->x - mos->draw_area.x,
			isect->y - mos->draw_area.y, isect->x, isect->y,
			isect->width, isect->height, GDK_RGB_DITHER_NONE, 0, 0);
	g_object_unref(scaled_pb);
	return;
fail:
	grey_fill(widget, isect);
	return;
}

static void draw_maps(struct gropes_state *state, GtkWidget *widget,
		      GdkRectangle *area, struct map_on_screen *mos_list)
{
	struct map_on_screen *mos;
	GdkGC *gc;
	GdkColor blue;

	gc = gdk_gc_new(GDK_DRAWABLE(widget->window));
	if (gc == NULL)
		return;
	blue.red = 0;
	blue.green = 0;
	blue.blue = 0xffff;
	blue.pixel = 0x0000ff;
	gdk_gc_set_foreground(gc, &blue);
	for (mos = mos_list; mos != NULL; mos = mos->next) {
		GdkRectangle isect;

		if (!gdk_rectangle_intersect(&mos->draw_area, area, &isect))
			continue;

		if (mos->map == NULL) {
			grey_fill(widget, &isect);
			continue;
		}

		if (mos->map_area.height != mos->draw_area.height ||
		    mos->map_area.width != mos->draw_area.width) {
			draw_single_map_scaled(widget, state->nav, mos, &isect);
		} else
			draw_single_map(widget, state->nav, mos, &isect);

		if (state->opt_draw_map_rectangles)
			gdk_draw_rectangle(widget->window, gc, FALSE, mos->draw_area.x,
					   mos->draw_area.y, mos->draw_area.width - 1,
					   mos->draw_area.height - 1);
	}
	g_object_unref(gc);
}

static int compare_draw_area(const void *arg1, const void *arg2)
{
	const struct map_on_screen *m1 = arg1, *m2 = arg2;

	if (m1->draw_area.y < m2->draw_area.y)
		return -1;
	if (m1->draw_area.y > m2->draw_area.y)
		return 1;
	if (m1->draw_area.x < m2->draw_area.x)
		return -1;
	if (m1->draw_area.x > m2->draw_area.x)
		return 1;
	return 0;
}

static void sort_map_list(struct map_on_screen **head)
{
	struct map_on_screen *start;

	/* A lame sorting algorithm, but as this is only used for
	 * debugging, I don't care that much about the performance */
	for (start = *head; start != NULL; start = start->next) {
		struct map_on_screen *smallest, **smallest_ptr = NULL, **prev;
		struct map_on_screen *mos;

		smallest = start;
		prev = &start->next;
		for (mos = start->next; mos != NULL; mos = mos->next) {
			if (compare_draw_area(mos, smallest) < 0) {
				smallest = mos;
				smallest_ptr = prev;
			}
			prev = &mos->next;
		}
		if (smallest != start) {
			/* Replace the current element's 'next' ptr */
			mos = start->next;
			start->next = smallest->next;
			if (mos == smallest)
				smallest->next = start;
			else
				smallest->next = mos;

			/* Replace the previous element's 'next' ptr */
			*head = smallest;
			if (smallest_ptr != &start->next)
				*smallest_ptr = start;

			start = smallest;
		}
		head = &start->next;
	}
}

static void calc_new_item_pos(struct gpsnav *nav, struct map_state *ms,
			      struct item_on_screen *item)
{
	struct gps_coord *pos = &item->pos;
	GdkRectangle *area;
	int r;

	gpsnav_get_metric_for_coord(ms->ref_map, pos, &item->mpos);
	area = &item->area;
	r = get_xy_on_screen(ms, &item->mpos, &area->x, &area->y);
	if (r < 0) {
		item->on_screen = 0;
		return;
	}
	area->x -= area->width / 2;
	area->y -= area->height / 2;
	item->on_screen = 1;
}

static void move_item_on_map(struct gropes_state *gs, struct map_state *ms,
			     struct item_on_screen *item, const struct gps_coord *pos)
{
	GdkRectangle *area;

	area = &item->area;
	if (pos == NULL) {
		/* If the position was already invalid, we don't have to
		 * do anything */
		if (!item->pos_valid)
			return;
		item->pos_valid = item->on_screen = 0;
	} else {
		GdkRectangle old_area;
		int was_valid;

		/* If the coordinates didn't change, we don't have to
		 * do anything */
		if (pos->la == item->pos.la &&
		    pos->lo == item->pos.lo &&
		    item->pos_valid)
			return;

		was_valid = item->pos_valid;
		item->pos_valid = 1;
		item->pos = *pos;
		old_area = *area;
		pthread_mutex_lock(&ms->mutex);
		calc_new_item_pos(gs->nav, ms, item);
		pthread_mutex_unlock(&ms->mutex);
		/* Now area contains the new item area */
		if (area->x == old_area.x && area->y == old_area.y &&
		    area->height == old_area.height &&
		    area->width == old_area.width)
			return;

		if (was_valid) {
			/* Invalidate old area */
			gtk_widget_queue_draw_area(ms->darea, old_area.x, old_area.y,
						   old_area.width, old_area.height);
		}
	}
	gtk_widget_queue_draw_area(ms->darea, area->x, area->y,
				   area->width, area->height);
}


static void change_map_center(struct gpsnav *nav, struct map_state *state,
			      const struct gps_mcoord *cent, double scale)
{
	struct map_on_screen *mos;
	struct gps_map **maps;
	struct gps_marea *marea;
	GdkRectangle draw_area;
	int width, height, r;

	pthread_mutex_lock(&state->mutex);

	free_mos_list(state->mos_list);
	state->mos_list = NULL;

	if (scale < 0.1)
		scale = 0.1;

	state->scale = scale;
	state->center_mpos = *cent;
	gpsnav_get_coord_for_metric(state->ref_map, cent, &state->center_pos);

	{
		char *coord;

		coord = fmt_coord(&state->center_pos, 0);
		printf("Center coord %s\n", coord);
		free(coord);
		coord = fmt_coord(&state->center_pos, 1);
		printf("             %s\n", coord);
		free(coord);
		coord = fmt_coord(&state->center_pos, 2);
		printf("             %s\n", coord);
		free(coord);

		printf("Scale %f\n", state->scale);
	}

	width = state->width;
	height = state->height;

	marea = &state->marea;
	marea->start.n = cent->n - height * scale / 2;
	marea->start.e = cent->e - width * scale / 2;
#if 1
	/* Round to pixel boundaries */
	marea->start.n -= fmod(marea->start.n, scale);
	marea->start.e -= fmod(marea->start.e, scale);
#endif
	marea->end.n = marea->start.n + height * scale;
	marea->end.e = marea->start.e + width * scale;

	/* Here we should already have a new ref map */
	if (state->me.pos_valid)
		calc_new_item_pos(nav, state, &state->me);

	maps = gpsnav_find_maps_for_marea(nav, state->ref_map, marea);
	if (maps == NULL) {
		/* FIXME: Try to find a new ref map for latlong area */
		printf("No maps found!\n");
		goto ret;
	}
	{
		int i;

		for (i = 0; maps[i] != NULL; i++);
		printf("%d map(s) found for area\n", i);
	}

	draw_area.x = draw_area.y = 0;
	draw_area.width = width;
	draw_area.height = height;
	r = generate_map_layout(nav, state->ref_map, maps, &draw_area, marea,
				state->scale, &draw_area, &state->mos_list, 0);
	free(maps);
	if (r < 0) {
		gps_error("generate_map_layout failed");
		free_mos_list(state->mos_list);
		state->mos_list = NULL;
		goto ret;
	}
	sort_map_list(&state->mos_list);
	for (mos = state->mos_list; mos != NULL; mos = mos->next) {
		GdkRectangle *sa, *ma;

		sa = &mos->draw_area;
		ma = &mos->map_area;
		if (mos->map != NULL) {
			int rescale = 0;
			char *fname;

			fname = get_map_fname(nav, mos->map);
			if (sa->width != ma->width ||
			    sa->height != ma->height)
				rescale = 1;
			printf("Map   %4d, %4d (%4d, %4d)  %cM %4d, %4d (%4d, %4d): %s\n",
			       sa->x, sa->y, sa->width, sa->height,
			       rescale ? 'S' : ' ', ma->x, ma->y,
			       ma->width, ma->height, fname);
			free(fname);
		} else {
			printf("Blank %4d, %4d (%4d, %4d)\n", sa->x, sa->y,
			       sa->width, sa->height);
		}
	}
	gtk_widget_queue_draw_area(state->darea, 0, 0,
				   state->darea->allocation.width,
				   state->darea->allocation.height);
ret:
	pthread_mutex_unlock(&state->mutex);
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

static void redraw_map_area(struct gropes_state *state, struct map_state *ms,
			    GtkWidget *widget, GdkRectangle *area)
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
	draw_maps(state, widget, area, ms->mos_list);
	if (ms->me.pos_valid && ms->me.on_screen &&
	    gdk_rectangle_intersect(area, &ms->me.area, &tmp))
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

static void scroll_map(struct gropes_state *state, struct map_state *ms,
		       int diff_x, int diff_y, double new_scale)
{
	struct gps_mcoord mcent;

	mcent = ms->center_mpos;
	mcent.n -= diff_y * ms->scale;
	mcent.e += diff_x * ms->scale;
	change_map_center(state->nav, ms, &mcent, new_scale);
}

static gboolean on_darea_clicked(GtkWidget *widget,
				 GdkEventButton *event,
				 gpointer user_data)
{
	struct map_state *map_state = user_data;
	double new_scale;
	int diff_x, diff_y;

	diff_x = event->x - widget->allocation.width / 2;
	diff_y = event->y - widget->allocation.height / 2;
	printf("Click %dx%d!\n", diff_x, diff_y);

	new_scale = map_state->scale;
	if (event->button == 2)
		new_scale /= 1.5;
	else if (event->button == 3)
		new_scale *= 1.5;

	scroll_map(&gropes_state, map_state, diff_x, diff_y, new_scale);

	return TRUE;
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

static gboolean on_darea_expose(GtkWidget *widget,
				GdkEventExpose *event,
				gpointer user_data)
{
	struct map_state *ms = user_data;
	GdkRectangle *expose_area = &event->area;

	printf("expose %d:%d (%d:%d)\n", expose_area->x, expose_area->y,
	       expose_area->width, expose_area->height);
	redraw_map_area(&gropes_state, ms, widget, expose_area);

	return TRUE;
}

static gboolean on_darea_configure(GtkWidget *widget,
				   GdkEventConfigure *event,
				   gpointer user_data)
{
	struct map_state *ms = user_data;

	printf("configure %d:%d (%d:%d)\n", event->x, event->y,
	       event->width, event->height);
	if (event->width != ms->width ||
	    event->height != ms->height) {
		ms->width = event->width;
		ms->height = event->height;
		change_map_center(gropes_state.nav, ms, &ms->center_mpos, ms->scale);
	}

	return TRUE;
}

static void gropes_shutdown(void)
{
	gpsnav_finish(gropes_state.nav);
	gtk_main_quit();
}

static void on_main_window_destroy(GtkWidget *widget, gpointer data)
{
	gropes_shutdown();
}

static void on_mode_change(GtkRadioAction *action, GtkRadioAction *current,
			   struct gropes_state *gs)
{
	int new_mode;

	new_mode = gtk_radio_action_get_current_value(action);
	printf("new mode %d\n", new_mode);
}

static void on_zoom_in(GtkAction *action, struct gropes_state *gs)
{
	struct map_state *ms;

	ms = &gs->big_map;
	printf("Zooming in\n");
	change_map_center(gs->nav, ms, &ms->center_mpos, ms->scale / 1.5);
}

static void on_zoom_out(GtkAction *action, struct gropes_state *gs)
{
	struct map_state *ms;

	ms = &gs->big_map;
	printf("Zooming out\n");
	change_map_center(gs->nav, ms, &ms->center_mpos, ms->scale * 1.5);
}

static void on_scroll(GtkAction *action, struct gropes_state *gs)
{
	const char *name;
	struct map_state *ms = &gs->big_map;

	name = gtk_action_get_name(action);
	if (strcmp(name, "ScrollUp") == 0)
		scroll_map(gs, ms, 0, -ms->height / 4, ms->scale);
	else if (strcmp(name, "ScrollDown") == 0)
		scroll_map(gs, ms, 0, ms->height / 4, ms->scale);
	else if (strcmp(name, "ScrollLeft") == 0)
		scroll_map(gs, ms, -ms->width / 4, 0, ms->scale);
	else if (strcmp(name, "ScrollRight") == 0)
		scroll_map(gs, ms, ms->width / 4, 0, ms->scale);
}

static void on_gps_connect(GtkAction *action, struct gropes_state *gs)
{
	GtkAction *other_act;

	printf("Connecting\n");
	if (gpsnav_connect(gs->nav) < 0)
                return;
	other_act = gtk_ui_manager_get_action(gs->ui_manager, "ui/MainMenu/GPSMenu/GPSDisconnect");
	gtk_action_set_sensitive(action, FALSE);
	gtk_action_set_sensitive(other_act, TRUE);
}

static void on_gps_disconnect(GtkAction *action, struct gropes_state *gs)
{
	GtkAction *other_act;

	printf("Disconnecting\n");
	gpsnav_disconnect(gs->nav);
	move_item_on_map(gs, &gs->big_map, &gs->big_map.me, NULL);
	other_act = gtk_ui_manager_get_action(gs->ui_manager, "ui/MainMenu/GPSMenu/GPSConnect");
	gtk_action_set_sensitive(action, FALSE);
	gtk_action_set_sensitive(other_act, TRUE);
}

static void on_gps_follow(GtkToggleAction *action, struct gropes_state *gs)
{
	printf("Setting follow mode to %d\n", gtk_toggle_action_get_active(action));
}

static void on_menu_exit(GtkAction *action, struct gropes_state *gs)
{
	gropes_shutdown();
}

static const GtkActionEntry menu_entries[] = {
	{ "FileMenu",	NULL,		   "_File" },
	{ "Exit", 	GTK_STOCK_QUIT,    "E_xit", "<control>Q", "Exit the program", G_CALLBACK(on_menu_exit) },
	{ "ModeMenu",	NULL,		   "_Mode" },
	{ "ViewMenu",	NULL,		   "_View" },
	{ "GPSMenu",	NULL,		   "_GPS" },
	{ "ZoomIn",	GTK_STOCK_ZOOM_IN, "Zoom _In", "plus", "Zoom into the map", G_CALLBACK(on_zoom_in) },
	{ "ZoomOut",	GTK_STOCK_ZOOM_OUT,"Zoom _Out", "minus", "Zoom away from the map", G_CALLBACK(on_zoom_out) },
	{ "ScrollLeft", NULL,		   "Scroll _Left",  "Left", "Scroll the map left", G_CALLBACK(on_scroll) },
	{ "ScrollRight", NULL,		   "Scroll _Right", "Right","Scroll the map right",G_CALLBACK(on_scroll) },
	{ "ScrollUp", NULL,		   "Scroll _Up",    "Up",   "Scroll the map up",   G_CALLBACK(on_scroll) },
	{ "ScrollDown", NULL,		   "Scroll _Down",  "Down", "Scroll the map down", G_CALLBACK(on_scroll) },
	{ "GPSConnect",	NULL,		   "_Connect", "C", "Connect to the GPS receiver", G_CALLBACK(on_gps_connect) },
	{ "GPSDisconnect",NULL,		   "_Disconnect", "D", "Disconnect from the GPS receiver", G_CALLBACK(on_gps_disconnect) },
};

static const GtkRadioActionEntry mode_entries[] = {
	{ "ModeTerrestrial",	NULL,	"_Terrestrial",	"<shift>T", "Switch to terrestrial mode", GROPES_MODE_TERRESTRIAL },
	{ "ModeNautical",	NULL,	"_Nautical",	"<shift>N", "Switch to nautical mode",	  GROPES_MODE_NAUTICAL },
};

static const GtkToggleActionEntry toggle_entries[] = {
	{ "GPSFollow",	NULL,   "_Follow", "F", "Center map automatically to GPS location", G_CALLBACK(on_gps_follow), FALSE },
};

static const char *ui_description =
"<ui>"
"  <menubar name='MainMenu'>"
"    <menu action='FileMenu'>"
"      <menuitem action='Exit'/>"
"    </menu>"
"    <menu action='ModeMenu'>"
"      <menuitem action='ModeTerrestrial'/>"
"      <menuitem action='ModeNautical'/>"
"    </menu>"
"    <menu action='GPSMenu'>"
"      <menuitem action='GPSConnect'/>"
"      <menuitem action='GPSDisconnect'/>"
"      <menuitem action='GPSFollow'/>"
"    </menu>"
"    <menu action='ViewMenu'>"
"      <menuitem action='ZoomIn'/>"
"      <menuitem action='ZoomOut'/>"
"      <separator/>"
"      <menuitem action='ScrollUp'/>"
"      <menuitem action='ScrollDown'/>"
"      <menuitem action='ScrollLeft'/>"
"      <menuitem action='ScrollRight'/>"
"    </menu>"
"  </menubar>"
"</ui>";

static GtkWidget *create_big_map_darea(GtkWidget *topwin, struct gropes_state *state)
{
	struct map_state *ms = &state->big_map;
	GtkWidget *darea;

	darea = gtk_drawing_area_new();
	gtk_widget_set_size_request(darea, DEFAULT_WIDTH, DEFAULT_HEIGHT);

	gtk_signal_connect(GTK_OBJECT(darea), "expose-event",
			   GTK_SIGNAL_FUNC(on_darea_expose), &state->big_map);
	gtk_signal_connect(GTK_OBJECT(darea), "configure-event",
			   GTK_SIGNAL_FUNC(on_darea_configure), &state->big_map);
	gtk_signal_connect(GTK_OBJECT(darea), "button-press-event",
			   GTK_SIGNAL_FUNC(on_darea_clicked), &state->big_map);

	gtk_widget_set_events(darea, GDK_EXPOSURE_MASK | GDK_PROPERTY_CHANGE_MASK |
			      GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);

	ms->darea = darea;

	return darea;
}

static int create_ui(GtkWidget *window, struct gropes_state *state)
{
	GtkWidget *vbox, *menu_bar, *big_map_darea;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GtkAction *act;
	GError *error;

//	gtk_signal_connect(GTK_OBJECT(window), "key-press-event",
//			   GTK_SIGNAL_FUNC(on_win_key_pressed), state);
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	action_group = gtk_action_group_new("MenuActions");
	gtk_action_group_add_actions(action_group, menu_entries,
				     G_N_ELEMENTS(menu_entries), state);
	gtk_action_group_add_toggle_actions(action_group, toggle_entries,
					    G_N_ELEMENTS(toggle_entries), state);
	gtk_action_group_add_radio_actions(action_group, mode_entries,
					   G_N_ELEMENTS(mode_entries), GROPES_MODE_TERRESTRIAL,
					   G_CALLBACK(on_mode_change), state);

	ui_manager = gtk_ui_manager_new();
	state->ui_manager = ui_manager;
	gtk_ui_manager_insert_action_group(ui_manager, action_group, 0);
	accel_group = gtk_ui_manager_get_accel_group(ui_manager);
	gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
	if (!gtk_ui_manager_add_ui_from_string(ui_manager, ui_description, -1, &error)) {
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
		return -1;
	}

	act = gtk_ui_manager_get_action(ui_manager, "ui/MainMenu/GPSMenu/GPSDisconnect");
	gtk_action_set_sensitive(act, FALSE);

	big_map_darea = create_big_map_darea(window, state);
	menu_bar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");

	gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 2);
	gtk_box_pack_end(GTK_BOX(vbox), big_map_darea, TRUE, TRUE, 2);

	return 0;
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
		move_item_on_map(gs, ms, item, NULL);
		gdk_threads_leave();
	} else {
		struct gps_coord pos;

		pos.la = fix->latitude;
		pos.lo = fix->longitude;
		gdk_threads_enter();
		move_item_on_map(gs, ms, item, &pos);
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

	create_ui(window, &gropes_state);

	gropes_state.big_map.me.area.height = gropes_state.big_map.me.area.width = 10;
	gropes_state.big_map.ref_map = ref_map;
	change_map_center(nav, &gropes_state.big_map, &center_mpos, scale);

	gtk_signal_connect(GTK_OBJECT(window), "destroy", GTK_SIGNAL_FUNC(on_main_window_destroy), NULL);

	gtk_widget_show_all(window);

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	return 0;
}
