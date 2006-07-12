#include <string.h>
#include <gtk/gtk.h>
#include "gropes.h"
#include "ui.h"

static void update_infoarea(struct map_state *ms, struct item_on_screen *item)
{
	char *location_color;
	char location[128];
	char *speed;
	char course[64];
	char *pos;

	if (item->pos_valid)
		location_color = g_markup_printf_escaped("grey");
	else
		location_color = g_markup_printf_escaped("red");

	pos = fmt_location(&item->pos);
	snprintf(location, sizeof(location), "<span size=\"large\" background=\"%s\">%s</span>",
					   location_color, pos);
	speed = g_markup_printf_escaped("<span size=\"large\">%.1f kt</span>", item->speed);
	snprintf(course, sizeof(course), "<span size=\"large\">%.1f&#0176;</span>", item->track);

	gtk_label_set_markup(GTK_LABEL(ms->info_area.location_display), location);
	gtk_label_set_markup(GTK_LABEL(ms->info_area.speed_display), speed);
	gtk_label_set_markup(GTK_LABEL(ms->info_area.course_display), course);

	g_free(pos);
	g_free(location_color);
	g_free(speed);
}

static const GtkActionEntry menu_entries[] = {
	{ "FileMenu",	NULL,		   "_File" },
	{ "Exit", 	GTK_STOCK_QUIT,    "E_xit", "<control>Q", "Exit the program", G_CALLBACK(on_menu_exit) },
	{ "ModeMenu",	NULL,		   "_Mode" },
	{ "ViewMenu",	NULL,		   "_View" },
	{ "GPSMenu",	NULL,		   "_GPS" },
	{ "ZoomIn",	GTK_STOCK_ZOOM_IN, "Zoom In", "F7", "Zoom into the map", G_CALLBACK(on_zoom_in) },
	{ "ZoomOut",	GTK_STOCK_ZOOM_OUT,"Zoom Out", "F8", "Zoom away from the map", G_CALLBACK(on_zoom_out) },
	{ "FullScreen", NULL,		   "Full Screen", "F6", "Full screen", G_CALLBACK(on_fullscreen) },
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
"      <menuitem action='FullScreen'/>"
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

int create_gtk_ui(struct gropes_state *gs)
{
	GtkWidget *cmd_area, *map_area, *menu_bar, *big_map_darea, *map_and_cmd_area, *main_win,
		  *vbox;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GtkAction *act;
	GError *error;
	struct map_state *ms = &gs->big_map;

	main_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(main_win), "Gropes");

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(main_win), vbox);
	cmd_area = gtk_vbox_new(TRUE, 10);
	map_and_cmd_area = gtk_hpaned_new();

	ms->info_area.location_frame = gtk_frame_new("Location");
	ms->info_area.location_display = gtk_label_new(NULL);
	gtk_label_set_use_markup(GTK_LABEL(ms->info_area.location_display), TRUE);
	gtk_label_set_markup(GTK_LABEL(ms->info_area.location_display),
			     "<span size=\"large\" background=\"red\">N\nE</span>");

	ms->info_area.speed_frame = gtk_frame_new("Speed");
	ms->info_area.speed_display = gtk_label_new(NULL);
	gtk_label_set_use_markup(GTK_LABEL(ms->info_area.speed_display), TRUE);
	gtk_label_set_markup(GTK_LABEL(ms->info_area.speed_display), 
			     "<span size=\"large\">0 kt</span>");

	ms->info_area.course_frame = gtk_frame_new("Course");
	ms->info_area.course_display = gtk_label_new(NULL);
	gtk_label_set_use_markup(GTK_LABEL(ms->info_area.course_display), TRUE);
	gtk_label_set_markup(GTK_LABEL(ms->info_area.course_display), 
			     "<span size=\"large\">0&#0176;</span>");

	ms->info_area.pointer_loc_frame = gtk_frame_new("Pointer location");
	ms->info_area.pointer_loc_display = gtk_label_new(NULL);
	gtk_label_set_use_markup(GTK_LABEL(ms->info_area.pointer_loc_display), TRUE);
	gtk_label_set_markup(GTK_LABEL(ms->info_area.pointer_loc_display), 
			     "<span size=\"large\">N\nE</span>");

	gtk_container_add(GTK_CONTAINER(ms->info_area.speed_frame), ms->info_area.speed_display);
	gtk_container_add(GTK_CONTAINER(ms->info_area.location_frame), ms->info_area.location_display);
	gtk_container_add(GTK_CONTAINER(ms->info_area.course_frame), ms->info_area.course_display);
	gtk_container_add(GTK_CONTAINER(ms->info_area.pointer_loc_frame), ms->info_area.pointer_loc_display);
	gtk_container_add(GTK_CONTAINER(cmd_area), GTK_WIDGET(ms->info_area.speed_frame));
	gtk_container_add(GTK_CONTAINER(cmd_area), GTK_WIDGET(ms->info_area.location_frame));
	gtk_container_add(GTK_CONTAINER(cmd_area), GTK_WIDGET(ms->info_area.course_frame));
	gtk_container_add(GTK_CONTAINER(cmd_area), GTK_WIDGET(ms->info_area.pointer_loc_frame));
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

	printf("%s %d\n", __FUNCTION__, __LINE__);
	big_map_darea = create_big_map_darea(gs);
	printf("%s %d\n", __FUNCTION__, __LINE__);
	menu_bar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");

	printf("%s %d\n", __FUNCTION__, __LINE__);
	gtk_paned_add2(GTK_PANED(map_and_cmd_area), big_map_darea);
	printf("%s %d\n", __FUNCTION__, __LINE__);
	gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 2);
	printf("%s %d\n", __FUNCTION__, __LINE__);
	gtk_box_pack_end(GTK_BOX(vbox), map_and_cmd_area, TRUE, TRUE, 2);
	printf("%s %d\n", __FUNCTION__, __LINE__);

	gtk_signal_connect(GTK_OBJECT(main_win), "destroy", GTK_SIGNAL_FUNC(on_main_window_destroy), NULL);
	printf("%s %d\n", __FUNCTION__, __LINE__);

	gtk_widget_show_all(main_win);
	printf("%s %d\n", __FUNCTION__, __LINE__);
	gs->big_map.me.update_info = update_infoarea;
	printf("%s %d\n", __FUNCTION__, __LINE__);

	return 0;
}

