#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gpsnav/gpsnav.h>
#include "gropes.h"
#include "mapcache.h"

static void add_mapcache_entry(struct gropes_state *gs, struct gropes_mapcache_entry *e)
{
	e->prev = NULL;
	e->next = gs->mc_head;
	if (gs->mc_head == NULL)
		gs->mc_tail = e;
	else
		gs->mc_head->prev = e;
	gs->mc_head = e;
}

static void remove_mapcache_entry(struct gropes_state *gs, struct gropes_mapcache_entry *e)
{
	if (e->prev != NULL)
		e->prev->next = e->next;
	else
		gs->mc_head = e->next;
	if (e->next != NULL)
		e->next->prev = e->prev;
	else
		gs->mc_tail = e->prev;
}

static void mapcache_sanity_check(struct gropes_state *gs)
{
	struct gropes_mapcache_entry *e, *prev;
	int size = 0;

	prev = NULL;
	for (e = gs->mc_head; e != NULL; e = e->next) {
		assert(e->prev == prev);
		size += e->size;
		prev = e;
	}
	assert(size == gs->mc_cur_size);
}

static void free_mapcache_entries(struct gropes_state *gs, unsigned int need_bytes)
{
	struct gropes_mapcache_entry *e;
	unsigned freed_bytes = 0;

	e = gs->mc_tail;
	while (e != NULL) {
		freed_bytes += e->size;
		remove_mapcache_entry(gs, e);
		g_object_unref(e->pb);
		free(e);
		e = gs->mc_tail;
		if (freed_bytes >= need_bytes) {
			assert(gs->mc_cur_size >= freed_bytes);
			gs->mc_cur_size -= freed_bytes;
			mapcache_sanity_check(gs);
			return;
		}
	}
	/* We should never reach this */
	assert(0);
}

int gropes_mapcache_add(struct gropes_state *gs, struct gps_map *map,
			GdkPixbuf *pb, double scale)
{
	unsigned int size;
	int mc_bytes_left, pb_w, pb_h, pb_bps;
	struct gropes_mapcache_entry *e;

	mapcache_sanity_check(gs);

	pb_w = gdk_pixbuf_get_width(pb);
	pb_h = gdk_pixbuf_get_height(pb);
	pb_bps = gdk_pixbuf_get_bits_per_sample(pb);
	size = pb_w * pb_h * pb_bps / 8 + sizeof(*e);
	if (size > gs->mc_max_size)
		return -1;
	mc_bytes_left = gs->mc_max_size - gs->mc_cur_size;
	if (size > mc_bytes_left)
		free_mapcache_entries(gs, size - mc_bytes_left);

	e = malloc(sizeof(*e));
	if (e == NULL)
		return -ENOMEM;
	e->map = map;
	e->pb = pb;
	e->size = size;
	e->scale = scale;

	gs->mc_cur_size += size;
	g_object_ref(e->pb);
	add_mapcache_entry(gs, e);
	mapcache_sanity_check(gs);

	return 0;
}

static struct gropes_mapcache_entry * find_mapcache_entry(struct gropes_state *gs, struct gps_map *map,
						       int x, int y, int width, int height, int bpp, double scale)
{
	struct gropes_mapcache_entry *e;
	int end_x, end_y;
	int pb_w, pb_h, pb_bps;

	end_x = x + width;
	end_y = y + height;
	for (e = gs->mc_head; e != NULL; e = e->next) {
		if (e->map != map)
			continue;
		if (e->x > x || e->y > y)
			continue;
		pb_w = gdk_pixbuf_get_width(e->pb);
		pb_h = gdk_pixbuf_get_height(e->pb);
		pb_bps = gdk_pixbuf_get_bits_per_sample(e->pb);
		if (e->x + pb_w < end_x || e->y + pb_h < end_y)
			continue;
		if (e->scale != scale)
			continue;
		if (bpp > 0 && pb_bps != bpp)
			continue;
		return e;
	}
	return NULL;
}

GdkPixbuf *gropes_mapcache_get(struct gropes_state *gs, struct gps_map *map,
				struct gps_pixel_buf *pb, double scale)
{
	struct gropes_mapcache_entry *e;

	mapcache_sanity_check(gs);
	printf("%s %d\n", __FUNCTION__, __LINE__);

	e = find_mapcache_entry(gs, map, pb->x, pb->y, pb->width, pb->height, pb->bpp, scale);
	printf("%s %d\n", __FUNCTION__, __LINE__);

	if (e == NULL)
		return NULL;

	printf("%s %d\n", __FUNCTION__, __LINE__);
	pb->x = e->x;
	pb->y = e->y;
	pb->width = gdk_pixbuf_get_width(e->pb);
	pb->height = gdk_pixbuf_get_height(e->pb);
	pb->bpp = gdk_pixbuf_get_bits_per_sample(e->pb);

	printf("%s %d\n", __FUNCTION__, __LINE__);
	if (gs->mc_head != e) {
		/* "Touch" the entry for the fancy-ass LRU algorithm */
		remove_mapcache_entry(gs, e);
		add_mapcache_entry(gs, e);
	}

	printf("%s %d\n", __FUNCTION__, __LINE__);

#if 0
	src_row_stride = e->width * e->bpp / 8;
	src = e->data + (y - e->y) * src_row_stride + (x - e->x) * e->bpp / 8;
	len = width * bpp / 8;

	for (line = 0; line < height; line++) {
		memcpy(data, src, len);
		data += row_stride;
		src += src_row_stride;
	}
#endif
	g_object_ref(e->pb);
	return e->pb;
}

void gpsnav_mapcache_purge(struct gropes_state *gs)
{
	if (gs->mc_cur_size)
		free_mapcache_entries(gs, gs->mc_cur_size);
}
