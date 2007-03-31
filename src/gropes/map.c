#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "gropes.h"

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
			if (scale_factor < 0.0025)
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
//	printf("%s %s --> %s  %4d, %4d --> %4d, %4d\n",
//	       name, ss, se, a->x, a->y, a->x + a->width, a->y + a->height);
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

//	print_area("map  ", &map->marea, &map_area);
//	print_area("isect", &isect, &map_draw_area);
//	{
//		char *fname = get_map_fname(nav, map);

//		printf("%s\n", fname);
//		free(fname);
//	}

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
#if 0
	printf("comp_res: %c%c%c%c\n",
	       comp_res & FILL_UP ? 'U' : ' ',
	       comp_res & FILL_DOWN ? 'D' : ' ',
	       comp_res & FILL_LEFT ? 'L' : ' ',
	       comp_res & FILL_RIGHT ? 'R' : ' ');
#endif

	r = add_mos_entry(head, map, &map_area, &map_draw_area);
	if (r)
		return r;

	head = &(*head)->next;
	depth++;
	if (comp_res & FILL_UP) {
		GdkRectangle new_area;

		new_area = *screen_area;
		new_area.height = map_draw_area.y - screen_area->y;
//		printf("Drawing upper\n");
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
//		printf("Drawing lower\n");
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
//		printf("Drawing left\n");
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
//		printf("Drawing right\n");
		r = generate_map_layout(nav, ref_map, maps, zero_area,
					zero_marea, scale, &new_area, head,
					depth);
		if (r)
			return r;
		head = get_next_head(head);
	}

	return 0;
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

void draw_maps(struct gropes_state *state, GtkWidget *widget,
	       struct map_on_screen *mos_list, const GdkRectangle *area)
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

		if (!gdk_rectangle_intersect(&mos->draw_area, (GdkRectangle *) area, &isect))
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


void change_map_center(struct gropes_state *gs, struct map_state *ms,
		       const struct gps_mcoord *cent, double scale)
{
	struct map_on_screen *mos;
	struct gps_map **maps;
	struct gps_marea *marea;
	GdkRectangle draw_area;
	int width, height, r;

	pthread_mutex_lock(&ms->mutex);

	free_mos_list(ms->mos_list);
	ms->mos_list = NULL;

	if (scale < 0.1)
		scale = 0.1;

	ms->scale = scale;
	ms->center_mpos = *cent;
	gpsnav_get_coord_for_metric(ms->ref_map, cent, &ms->center_pos);

#if 0
	{
		char *coord;

		coord = fmt_coord(&ms->center_pos, 0);
		printf("Center coord %s\n", coord);
		free(coord);
		coord = fmt_coord(&ms->center_pos, 1);
		printf("             %s\n", coord);
		free(coord);
		coord = fmt_coord(&ms->center_pos, 2);
		printf("             %s\n", coord);
		free(coord);

		printf("Scale %f\n", ms->scale);
	}
#endif

	width = ms->width;
	height = ms->height;

	marea = &ms->marea;
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
	if (ms->me.pos_valid)
		calc_item_pos(gs, ms, &ms->me);

	maps = gpsnav_find_maps_for_marea(gs->nav, ms->ref_map, marea);
	if (maps == NULL) {
		/* FIXME: Try to find a new ref map for latlong area */
//		printf("No maps found!\n");
		goto ret;
	}

#if 0
	{
		int i;

		for (i = 0; maps[i] != NULL; i++);
		printf("%d map(s) found for area\n", i);
	}
#endif

	draw_area.x = draw_area.y = 0;
	draw_area.width = width;
	draw_area.height = height;
	r = generate_map_layout(gs->nav, ms->ref_map, maps, &draw_area, marea,
				ms->scale, &draw_area, &ms->mos_list, 0);
	free(maps);
	if (r < 0) {
		gps_error("generate_map_layout failed");
		free_mos_list(ms->mos_list);
		ms->mos_list = NULL;
		goto ret;
	}
	sort_map_list(&ms->mos_list);
	for (mos = ms->mos_list; mos != NULL; mos = mos->next) {
		GdkRectangle *sa, *ma;

		sa = &mos->draw_area;
		ma = &mos->map_area;
		if (mos->map != NULL) {
			int rescale = 0;
			char *fname;

			fname = get_map_fname(gs->nav, mos->map);
			if (sa->width != ma->width ||
			    sa->height != ma->height)
				rescale = 1;
#if 0
			printf("Map   %4d, %4d (%4d, %4d)  %cM %4d, %4d (%4d, %4d): %s\n",
			       sa->x, sa->y, sa->width, sa->height,
			       rescale ? 'S' : ' ', ma->x, ma->y,
			       ma->width, ma->height, fname);
#endif
			free(fname);
		}
#if 0
		else {
			printf("Blank %4d, %4d (%4d, %4d)\n", sa->x, sa->y,
			       sa->width, sa->height);
		}
#endif
	}
	gtk_widget_queue_draw_area(ms->darea, 0, 0,
				   ms->darea->allocation.width,
				   ms->darea->allocation.height);
ret:
	pthread_mutex_unlock(&ms->mutex);
}
