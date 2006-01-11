#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <gpsnav/gpsnav.h>
#include <gpsnav/pixcache.h>
#include <gpsnav/map.h>

static void add_pixcache_entry(struct gpsnav *nav, struct gps_pixcache_entry *e)
{
	e->prev = NULL;
	e->next = nav->pc_head;
	if (nav->pc_head == NULL)
		nav->pc_tail = e;
	else
		nav->pc_head->prev = e;
	nav->pc_head = e;
}

#if 0
static void switch_pixcache_entries(struct gpsnav *nav, struct gps_pixcache_entry *e1,
				    struct gps_pixcache_entry *e2)
{
	struct gps_pixcache_entry *tmp;

	if (e1->prev != NULL)
		e1->prev->next = e2->next;
	else
		nav->pc_head = e2;
	if (e1->next != NULL)
		e1->next->prev = e2->prev;
	else
		nav->pc_tail = e2;

	if (e2->prev != NULL)
		e2->prev->next = e1->next;
	else
		nav->pc_head = e1;
	if (e2->next != NULL)
		e2->next->prev = e1->prev;
	else
		nav->pc_tail = e1;

	tmp = e1->next;
	e1->next = e2->next;
	e2->next = tmp;

	tmp = e1->prev;
	e1->prev = e2->prev;
	e2->prev = tmp;
}
#endif

static void remove_pixcache_entry(struct gpsnav *nav, struct gps_pixcache_entry *e)
{
	if (e->prev != NULL)
		e->prev->next = e->next;
	else
		nav->pc_head = e->next;
	if (e->next != NULL)
		e->next->prev = e->prev;
	else
		nav->pc_tail = e->prev;
}

static void pixcache_sanity_check(struct gpsnav *nav)
{
	struct gps_pixcache_entry *e, *prev;
	int size = 0;

	prev = NULL;
	for (e = nav->pc_head; e != NULL; e = e->next) {
		assert(e->prev == prev);
		size += e->size;
		prev = e;
	}
	assert(size == nav->pc_cur_size);
}

static void free_pixcache_entries(struct gpsnav *nav, unsigned int need_bytes)
{
	struct gps_pixcache_entry *e;
	unsigned freed_bytes = 0;

	e = nav->pc_tail;
	while (e != NULL) {
		freed_bytes += e->size;
		remove_pixcache_entry(nav, e);
		free(e->pb.data);
		free(e);
		e = nav->pc_tail;
		if (freed_bytes >= need_bytes) {
			assert(nav->pc_cur_size >= freed_bytes);
			nav->pc_cur_size -= freed_bytes;
			pixcache_sanity_check(nav);
			return;
		}
	}
	/* We should never reach this */
	assert(0);
}

int gpsnav_pixcache_add(struct gpsnav *nav, struct gps_map *map,
			struct gps_pixel_buf *pb)
{
	unsigned int size;
	int pc_bytes_left;
	struct gps_pixcache_entry *e;

	pixcache_sanity_check(nav);

	size = pb->width * pb->height * pb->bpp / 8 + sizeof(*e);
	if (size > nav->pc_max_size)
		return -1;
	pc_bytes_left = nav->pc_max_size - nav->pc_cur_size;
	if (size > pc_bytes_left)
		free_pixcache_entries(nav, size - pc_bytes_left);

	e = malloc(sizeof(*e));
	if (e == NULL)
		return -ENOMEM;
	e->map = map;
	e->pb = *pb;
	e->size = size;

	nav->pc_cur_size += size;
	add_pixcache_entry(nav, e);
	pixcache_sanity_check(nav);

	return 0;
}

static struct gps_pixcache_entry * find_pixcache_entry(struct gpsnav *nav, struct gps_map *map,
						       int x, int y, int width, int height, int bpp)
{
	struct gps_pixcache_entry *e;
	int end_x, end_y;

	end_x = x + width;
	end_y = y + height;
	for (e = nav->pc_head; e != NULL; e = e->next) {
		if (e->map != map)
			continue;
		if (e->pb.x > x || e->pb.y > y)
			continue;
		if (e->pb.x + e->pb.width < end_x || e->pb.y + e->pb.height < end_y)
			continue;
		if (bpp > 0 && e->pb.bpp != bpp)
			continue;
		return e;
	}
	return NULL;
}

int gpsnav_pixcache_get(struct gpsnav *nav, struct gps_map *map,
			struct gps_pixel_buf *pb)
{
	struct gps_pixcache_entry *e;

	pixcache_sanity_check(nav);
	e = find_pixcache_entry(nav, map, pb->x, pb->y, pb->width, pb->height, pb->bpp);
	if (e == NULL)
		return -1;

	if (nav->pc_head != e) {
		/* "Touch" the entry for the fancy-ass LRU algorithm */
		remove_pixcache_entry(nav, e);
		add_pixcache_entry(nav, e);
	}

	if (pb->data == NULL) {
		*pb = e->pb;
		pb->can_free = 0;
		return 0;
	}

	assert(0);
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
	return 0;
}

void gpsnav_pixcache_purge(struct gpsnav *nav)
{
	if (nav->pc_cur_size)
		free_pixcache_entries(nav, nav->pc_cur_size);
}
