#include <string.h>
#include <gps.h>
#include <gtk/gtk.h>
#include "gropes.h"
#include "ui.h"

#define DEFAULT_WIDTH		800
#define DEFAULT_HEIGHT		480

char *fmt_location(const struct gps_coord *coord)
{
	char buf[64];
	int deg_la, deg_lo, min_la, min_lo;
	double sec_la, sec_lo;
	char ns, ew;

	ns = coord->la > 0 ? 'N' : 'S';
	ew = coord->lo > 0 ? 'E' : 'W';
	deg_la = floor(coord->la);
	deg_lo = floor(coord->lo);
	min_la = floor((coord->la - deg_la) * 60.0);
	min_lo = floor((coord->lo - deg_lo) * 60.0);
	sec_la = (coord->la - deg_la - min_la / 60.0) * 3600.0;
	sec_lo = (coord->lo - deg_lo - min_lo / 60.0) * 3600.0;

	sprintf(buf, "%d&#0176;%02d.%03d&apos;%c\n%d&#0176;%02d.%03d&apos;%c",
		deg_la, min_la, (int) (sec_la * 1000 / 60), ns,
		deg_lo, min_lo, (int) (sec_lo * 1000 / 60), ew);

	return strdup(buf);
}

static void scroll_map(struct gropes_state *gs, struct map_state *ms,
			int diff_x, int diff_y, double new_scale)
{
	struct gps_mcoord mcent;

	mcent = ms->center_mpos;
	mcent.n -= diff_y * ms->scale;
	mcent.e += diff_x * ms->scale;
	change_map_center(gs, ms, &mcent, new_scale);
}

gboolean on_darea_clicked(GtkWidget *widget,
				 GdkEventButton *event,
				 gpointer user_data)
{
	struct map_state *map_state = user_data;
	double new_scale;
	int diff_x, diff_y;
	GtkToggleAction *action;

	action = gtk_ui_manager_get_action(gropes_state.ui_manager,
					   "ui/MainMenu/GPSMenu/GPSFollow");
	gropes_state.opt_follow_gps = 0;
	gtk_toggle_action_set_active(action, FALSE);

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

gboolean on_pointer_motion(GtkWidget *widget,
				  GdkEventMotion *event,
				  gpointer user_data)
{
	struct map_state *ms = user_data;
	int diff_x, diff_y;
	double mdiff_x, mdiff_y, pangle, pdist, ptime;
	struct gps_mcoord mpoint;
	struct gps_coord point;
	char pointer_loc[64], *pos;
	int hours, mins, secs;

	diff_x = event->x - widget->allocation.width / 2;
	diff_y = event->y - widget->allocation.height / 2;
	mpoint = ms->center_mpos;
	mpoint.n -= diff_y * ms->scale;
	mpoint.e += diff_x * ms->scale;

	mdiff_x = mpoint.e - ms->me.mpos.e;
	mdiff_y = mpoint.n - ms->me.mpos.n;
	pdist = sqrt(mdiff_x * mdiff_x + mdiff_y * mdiff_y) /
		(1000 * KNOTS_TO_KPH);
	pangle = fabs(360 * atan(mdiff_y/mdiff_x) / (2 * PI));
	if (mdiff_x > 0 && mdiff_y > 0)
		pangle = 90 - pangle;
	if (mdiff_x > 0 && mdiff_y < 0)
		pangle = 90 + pangle;
	if (mdiff_x < 0 && mdiff_y < 0)
		pangle = 90 - pangle + 180;
	if (mdiff_x < 0 && mdiff_y > 0)
		pangle = 270 + pangle;

	if (mdiff_x == 0)
		pangle = mdiff_y > 0 ? 0 : 180;
	if (mdiff_y == 0)
		pangle = mdiff_x > 0 ? 90 : 270;

	if (ms->me.speed)
		ptime = pdist / ms->me.speed;
	else
		ptime = 0;

	hours = floor(ptime);
	mins = floor((ptime - hours) * 60.0);
	secs = (ptime - hours - mins / 60.0) * 3600.0;

	gpsnav_get_coord_for_metric(ms->ref_map, &mpoint, &point);

	pos = fmt_location(&point);
	sprintf(pointer_loc, "<span size=\"large\">%s\n%.1lf\n%.1lf\n%d:%.2d:%.2d</span>",
		pos, pdist, pangle, hours, mins, secs);

	gtk_label_set_markup(GTK_LABEL(ms->info_area.pointer_loc_display),
			     pointer_loc);

	g_free(pos);

	return TRUE;
}

gboolean on_darea_expose(GtkWidget *widget,
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

gboolean on_darea_configure(GtkWidget *widget,
				   GdkEventConfigure *event,
				   gpointer user_data)
{
	struct map_state *ms = user_data;

	printf("configure %d:%d (%d:%d)\n", event->x, event->y,
	       event->width, event->height);
	if (event->width != ms->width ||
	    event->height != ms->height &&
	    ms->ref_map) {
		ms->width = event->width;
		ms->height = event->height;
	printf("%s %d\n", __FUNCTION__, __LINE__);
		change_map_center(&gropes_state, ms, &ms->center_mpos, ms->scale);
	printf("%s %d\n", __FUNCTION__, __LINE__);
	}

	printf("%s %d\n", __FUNCTION__, __LINE__);
	return TRUE;
}

void on_mode_change(GtkRadioAction *action, GtkRadioAction *current,
			   struct gropes_state *gs)
{
	int new_mode;

	new_mode = gtk_radio_action_get_current_value(action);
	printf("new mode %d\n", new_mode);
}

void on_fullscreen(GtkAction *action, struct gropes_state *gs)
{
	printf("Full screen\n");
}

void on_zoom_in(GtkAction *action, struct gropes_state *gs)
{
	struct map_state *ms;

	ms = &gs->big_map;
	printf("Zooming in\n");
	change_map_center(gs, ms, &ms->center_mpos, ms->scale / 1.5);
}

void on_zoom_out(GtkAction *action, struct gropes_state *gs)
{
	struct map_state *ms;

	ms = &gs->big_map;
	printf("Zooming out\n");
	change_map_center(gs, ms, &ms->center_mpos, ms->scale * 1.5);
}

void on_scroll(GtkAction *action, struct gropes_state *gs)
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

void on_gps_connect(GtkAction *action, struct gropes_state *gs)
{
	GtkAction *other_act;

	printf("Connecting\n");
	if (gpsnav_connect(gs->nav) < 0)
                return;
	other_act = gtk_ui_manager_get_action(gs->ui_manager, "ui/MainMenu/GPSMenu/GPSDisconnect");
	gtk_action_set_sensitive(action, FALSE);
	gtk_action_set_sensitive(other_act, TRUE);
}

void on_gps_disconnect(GtkAction *action, struct gropes_state *gs)
{
	GtkAction *other_act;

	printf("Disconnecting\n");
	gpsnav_disconnect(gs->nav);
	move_item(gs, &gs->big_map, &gs->big_map.me, NULL);
	other_act = gtk_ui_manager_get_action(gs->ui_manager, "ui/MainMenu/GPSMenu/GPSConnect");
	gtk_action_set_sensitive(action, FALSE);
	gtk_action_set_sensitive(other_act, TRUE);
}

void on_gps_follow(GtkToggleAction *action, struct gropes_state *gs)
{
	struct map_state *ms = &gs->big_map;

	printf("Setting follow mode to %d\n", gtk_toggle_action_get_active(action));
	gs->opt_follow_gps = gtk_toggle_action_get_active(action);
	if (ms->me.pos_valid)
		change_map_center(gs, ms, &ms->me.mpos, ms->scale);
}

void on_menu_exit(GtkAction *action, struct gropes_state *gs)
{
	gropes_shutdown();
}

GtkWidget *create_big_map_darea(struct gropes_state *state)
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
	gtk_signal_connect(GTK_OBJECT(darea), "motion-notify-event",
			   GTK_SIGNAL_FUNC(on_pointer_motion), &state->big_map);

	gtk_widget_set_events(darea, GDK_EXPOSURE_MASK | GDK_PROPERTY_CHANGE_MASK |
			      GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK |
			      GDK_POINTER_MOTION_MASK);

	ms->darea = darea;

	return darea;
}

void on_main_window_destroy(GtkWidget *widget, gpointer data)
{
	gropes_shutdown();
}
