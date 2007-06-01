#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "gropes.h"
#include "ui.h"

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

static void item_save_track(struct item_on_screen *item)
{
	struct item_track *point;

	point = malloc(sizeof(struct item_track));
	if (point == NULL)
		return;

	memcpy(&point->mpos, &item->mpos, sizeof(struct gps_mcoord));
	point->next = item->track;
	item->track = point;
}

void item_draw_track(struct item_on_screen *item, int draw)
{
	item->draw_track = draw;
}

void item_clear_track(struct item_on_screen *item)
{
	struct item_track *point, *next;

	next = item->track;

	while (next) {
		point = next;
		next = point->next;
		free(point);
	}

	item->track = NULL;
}

void move_item(struct gropes_state *gs, struct map_state *ms,
	       struct item_on_screen *item, const struct gps_coord *pos,
	       struct gps_speed *speed)
{
	GdkRectangle *area;

	area = &item->area;

	if (pos == NULL) {
		/* If the position was already invalid, we don't have to
		 * do anything */
		item->pos_valid = item->on_screen = 0;
		if (item->update_info)
			item->update_info(ms, item);
	} else {
		GdkRectangle old_area;
		int was_valid;

		/* If the coordinates didn't change, we don't have to
		 * do anything */
		if (pos->la == item->pos.la &&
		    pos->lo == item->pos.lo &&
		    speed->speed == item->speed.speed &&
		    speed->track == item->speed.track &&
		    item->pos_valid)
			return;

		was_valid = item->pos_valid;
		item->pos_valid = 1;
		item->pos = *pos;
		item->speed = *speed;
		old_area = *area;
		if (item->update_info)
			item->update_info(ms, item);
		pthread_mutex_lock(&ms->mutex);
		calc_item_pos(gs, ms, item);
		item_save_track(item);
		pthread_mutex_unlock(&ms->mutex);
		/* Change map center if item is not on screen */
		if (gs->opt_follow_gps)
			change_map_center(gs, ms, &item->mpos, ms->scale);
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
