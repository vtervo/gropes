#include <assert.h>
#include "gropes.h"


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


void calc_item_pos(struct gropes_state *gs, struct map_state *ms,
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

void move_item(struct gropes_state *gs, struct map_state *ms,
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
		calc_item_pos(gs, ms, item);
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
	item->update_info(item);
}
