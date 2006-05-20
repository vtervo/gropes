#include <string.h>
#include <gtk/gtk.h>

#include "gropes.h"

#define DEFAULT_WIDTH		800
#define DEFAULT_HEIGHT		480

struct ui_info_area {
	GtkWidget *speed_frame;
	GtkWidget *speed_display;
	GtkWidget *location_frame;
	GtkWidget *location_display;
	GtkWidget *course_frame;
	GtkWidget *course_display;
};

static struct ui_info_area info_area;

static void update_infoarea(struct item_on_screen *item)
{
	char *location_color;
	char *location;
	char *speed;
	char *course;

	if (item->pos_valid)
		location_color = g_markup_printf_escaped("grey");
	else 
		location_color = g_markup_printf_escaped("red");

	location = g_markup_printf_escaped("<span size=\"xx-large\" background=\"%s\">La %.4f\nLo %.4f</span>",
				location_color, item->pos.la, item->pos.lo);
	speed = g_markup_printf_escaped("<span size=\"xx-large\">%.1f kt</span>", item->speed);
	course = g_markup_printf_escaped("<span size=\"xx-large\">%.1f</span>", item->track);

	gtk_label_set_markup(GTK_LABEL(info_area.location_display), location);
	gtk_label_set_markup(GTK_LABEL(info_area.speed_display), speed);
	gtk_label_set_markup(GTK_LABEL(info_area.course_display), course);

	g_free(location_color);
	g_free(location);
	g_free(speed);
	g_free(course);
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

static gboolean on_darea_clicked(GtkWidget *widget,
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
		change_map_center(&gropes_state, ms, &ms->center_mpos, ms->scale);
	}

	return TRUE;
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
	change_map_center(gs, ms, &ms->center_mpos, ms->scale / 1.5);
}

static void on_zoom_out(GtkAction *action, struct gropes_state *gs)
{
	struct map_state *ms;

	ms = &gs->big_map;
	printf("Zooming out\n");
	change_map_center(gs, ms, &ms->center_mpos, ms->scale * 1.5);
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
	move_item(gs, &gs->big_map, &gs->big_map.me, NULL);
	other_act = gtk_ui_manager_get_action(gs->ui_manager, "ui/MainMenu/GPSMenu/GPSConnect");
	gtk_action_set_sensitive(action, FALSE);
	gtk_action_set_sensitive(other_act, TRUE);
}

static void on_gps_follow(GtkToggleAction *action, struct gropes_state *gs)
{
	printf("Setting follow mode to %d\n", gtk_toggle_action_get_active(action));
	gs->opt_follow_gps = gtk_toggle_action_get_active(action);
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

static void on_main_window_destroy(GtkWidget *widget, gpointer data)
{
	gropes_shutdown();
}

int create_ui(struct gropes_state *gs)
{
	GtkWidget *cmd_area, *map_area, *menu_bar, *big_map_darea, *map_and_cmd_area, *main_win,
		  *vbox;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GtkAction *act;
	GError *error;

	main_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(main_win), "Gropes");

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(main_win), vbox);
	cmd_area = gtk_vbox_new(TRUE, 10);
	map_and_cmd_area = gtk_hpaned_new();

	info_area.location_frame = gtk_frame_new("Location");
	info_area.location_display = gtk_label_new(NULL);
	gtk_label_set_use_markup(GTK_LABEL(info_area.location_display), TRUE);
	gtk_label_set_markup(GTK_LABEL(info_area.location_display),
			     "<span size=\"x-large\" background=\"red\">La\nLo</span>");

	info_area.speed_frame = gtk_frame_new("Speed");
	info_area.speed_display = gtk_label_new(NULL);
	gtk_label_set_use_markup(GTK_LABEL(info_area.speed_display), TRUE);
	gtk_label_set_markup(GTK_LABEL(info_area.speed_display), 
			     "<span size=\"xx-large\">0 kt</span>");

	info_area.course_frame = gtk_frame_new("Course");
	info_area.course_display = gtk_label_new(NULL);
	gtk_label_set_use_markup(GTK_LABEL(info_area.course_display), TRUE);
	gtk_label_set_markup(GTK_LABEL(info_area.course_display), 
			     "<span size=\"xx-large\">0 deg</span>");

	gtk_container_add(GTK_CONTAINER(info_area.speed_frame), info_area.speed_display);
	gtk_container_add(GTK_CONTAINER(info_area.location_frame), info_area.location_display);
	gtk_container_add(GTK_CONTAINER(info_area.course_frame), info_area.course_display);
	gtk_container_add(GTK_CONTAINER(cmd_area), GTK_WIDGET(info_area.speed_frame));
	gtk_container_add(GTK_CONTAINER(cmd_area), GTK_WIDGET(info_area.location_frame));
	gtk_container_add(GTK_CONTAINER(cmd_area), GTK_WIDGET(info_area.course_frame));
	gtk_paned_add1(GTK_PANED(map_and_cmd_area), cmd_area);

	action_group = gtk_action_group_new("MenuActions");
	gtk_action_group_add_actions(action_group, menu_entries,
				     G_N_ELEMENTS(menu_entries), gs);
	gtk_action_group_add_toggle_actions(action_group, toggle_entries,
					    G_N_ELEMENTS(toggle_entries), gs);
	gtk_action_group_add_radio_actions(action_group, mode_entries,
					   G_N_ELEMENTS(mode_entries), GROPES_MODE_TERRESTRIAL,
					   G_CALLBACK(on_mode_change), gs);

	ui_manager = gtk_ui_manager_new();
	gs->ui_manager = ui_manager;
	gtk_ui_manager_insert_action_group(ui_manager, action_group, 0);
	accel_group = gtk_ui_manager_get_accel_group(ui_manager);
	gtk_window_add_accel_group(GTK_WINDOW(main_win), accel_group);
	if (!gtk_ui_manager_add_ui_from_string(ui_manager, ui_description, -1, &error)) {
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
		return -1;
	}

	act = gtk_ui_manager_get_action(ui_manager, "ui/MainMenu/GPSMenu/GPSDisconnect");
	gtk_action_set_sensitive(act, FALSE);

	big_map_darea = create_big_map_darea(main_win, gs);
	menu_bar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");

	gtk_paned_add2(GTK_PANED(map_and_cmd_area), big_map_darea);
	gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 2);
	gtk_box_pack_end(GTK_BOX(vbox), map_and_cmd_area, TRUE, TRUE, 2);

	gtk_signal_connect(GTK_OBJECT(main_win), "destroy", GTK_SIGNAL_FUNC(on_main_window_destroy), NULL);

	gtk_widget_show_all(main_win);
	gs->big_map.me.update_info = update_infoarea;

	return 0;
}

