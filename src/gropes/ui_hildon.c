#include <hildon-widgets/hildon-program.h>
#include <gtk/gtk.h>
#include <libosso.h>

#include "gropes.h"

static HildonWindow *main_view;
static osso_context_t *osso;

static void update_infoarea(struct map_state *ms, struct item_on_screen *item)
{
	char *location_color;
	char location[128];
	char *speed;
	char course[128];
	char *pos;

	if (item->pos_valid)
		location_color = "";
	else
		location_color = g_markup_printf_escaped(" background=\"red\"");

	pos = fmt_location(&item->pos);
	snprintf(location, sizeof(location), "<span size=\"large\"%s>%s</span>",
					   location_color, pos);
	speed = g_markup_printf_escaped("<span size=\"large\">%.1f kt</span>", item->speed.speed);
	snprintf(course, sizeof(course), "<span size=\"large\">%.1f&#0176;</span>", item->speed.track);

	gtk_label_set_markup(GTK_LABEL(ms->info_area.location_display), location);
	gtk_label_set_markup(GTK_LABEL(ms->info_area.speed_display), speed);
	gtk_label_set_markup(GTK_LABEL(ms->info_area.course_display), course);

	g_free(pos);
	g_free(location_color);
	g_free(speed);

	osso_display_blanking_pause(osso);
}

static void on_hildon_fullscreen(GtkToggleAction *action,
				     struct gropes_state *gs)
{
	if (gtk_toggle_action_get_active(action))
		gtk_window_fullscreen(GTK_WINDOW(main_view));
	else
		gtk_window_unfullscreen(GTK_WINDOW(main_view));
}

static const GtkActionEntry menu_entries[] = {
	{ "FileMenu",	NULL,		   "_File" },
	{ "Exit", 	GTK_STOCK_QUIT,    "E_xit", "<control>Q", "Exit the program", G_CALLBACK(on_menu_exit) },
	{ "ModeMenu",	NULL,		   "_Mode" },
	{ "ViewMenu",	NULL,		   "_View" },
	{ "GPSMenu",	NULL,		   "_GPS" },
	{ "ZoomIn",	GTK_STOCK_ZOOM_IN, "Zoom In", "F7", "Zoom into the map", G_CALLBACK(on_zoom_in) },
	{ "ZoomOut",	GTK_STOCK_ZOOM_OUT,"Zoom Out", "F8", "Zoom away from the map", G_CALLBACK(on_zoom_out) },
	{ "FullScreen", NULL,		   "Full Screen", "F6", "Full screen", G_CALLBACK(on_hildon_fullscreen) },
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
	{ "GPSFollow",	NULL,	"_Follow", "F", "Center map automatically to GPS location", G_CALLBACK(on_gps_follow), FALSE },
	{ "FullScreen", NULL,	"Full Screen", "F6", "Full screen", G_CALLBACK(on_hildon_fullscreen), FALSE },
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


int create_hildon_ui(struct gropes_state *gs)
{
	HildonProgram *app;
	GtkWidget *cmd_area, *map_area, *menu_bar, *big_map_darea, *map_and_cmd_area,
		  *vbox, *main_menu;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GtkAction *act;
	GError *error;
	struct map_state *ms = &gs->big_map;

	app = HILDON_PROGRAM(hildon_program_get_instance());
	g_set_application_name(("Gropes"));
	main_view = HILDON_WINDOW(hildon_window_new());

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(main_view), vbox);
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

	big_map_darea = create_big_map_darea(gs);
	hildon_program_add_window(app, main_view);

	/* Generate menus */
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
	gtk_window_add_accel_group(GTK_WINDOW(main_view), accel_group);
	if (!gtk_ui_manager_add_ui_from_string(ui_manager, ui_description, -1, &error)) {
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
		return -1;
	}

	act = gtk_ui_manager_get_action(ui_manager, "ui/MainMenu/GPSMenu/GPSDisconnect");
	gtk_action_set_sensitive(act, FALSE);

#if 1
	main_menu = GTK_MENU(gtk_menu_new());
	hildon_window_set_menu(main_view, main_menu);
	gtk_widget_reparent(gtk_ui_manager_get_widget(ui_manager, "/MainMenu/FileMenu"), main_menu);
	gtk_widget_reparent(gtk_ui_manager_get_widget(ui_manager, "/MainMenu/ModeMenu"), main_menu);
	gtk_widget_reparent(gtk_ui_manager_get_widget(ui_manager, "/MainMenu/GPSMenu"), main_menu);
	gtk_widget_reparent(gtk_ui_manager_get_widget(ui_manager, "/MainMenu/ViewMenu"), main_menu);
#endif

//	menu_bar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");
	gtk_paned_add2(GTK_PANED(map_and_cmd_area), big_map_darea);
//	gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 2);
//	gtk_box_pack_end(GTK_BOX(vbox), map_and_cmd_area, TRUE, TRUE, 2);
	gtk_container_add(GTK_CONTAINER(vbox), map_and_cmd_area);

	gtk_widget_show_all(GTK_WIDGET(main_view));
	gs->big_map.me.update_info = update_infoarea;
	osso = osso_initialize("gropes", "0.0.2", TRUE, NULL);
	if (osso == NULL)
		return -1;

	return 0;
}
