#include <stdlib.h>
#include <gtk/gtk.h>

#include "gropes-maptool-ui.h"

static void on_menu_exit(GtkAction *action, struct maptool_state ms)
{
	gtk_main_quit();
}

static void on_main_window_destroy(GtkWidget *widget, gpointer data)
{
	gtk_main_quit();
}

static const GtkActionEntry menu_entries[] = {
	{ "FileMenu",	NULL,		"_File" },
	{ "Exit",	GTK_STOCK_QUIT,	"E_xit", "<control>X", "Exit maptool", G_CALLBACK(on_menu_exit) },
};

static const char *ui_description =
"<ui>"
"  <menubar name='MainMenu'>"
"    <menu action='FileMenu'>"
"      <menuitem action='Exit'/>"
"    </menu>"
"  </menubar>"
"</ui>";

static int create_maptool_ui(struct maptool_state *ms)
{
	GtkWidget *menu_bar, *main_win, *vbox, *main_frame;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GError *error;

	main_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(main_win), "Gropes maptool");
	vbox = gtk_vbox_new(FALSE, 0);
	main_frame = gtk_frame_new("Frame");
	gtk_container_add(GTK_CONTAINER(main_win), vbox);

	action_group = gtk_action_group_new("MenuActions");
	gtk_action_group_add_actions(action_group, menu_entries,
				     G_N_ELEMENTS(menu_entries), ms);

	ui_manager = gtk_ui_manager_new();

	if (!gtk_ui_manager_add_ui_from_string(ui_manager, ui_description, -1, &error)) {
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
		return -1;
	}

	menu_bar = gtk_ui_manager_get_widget(ui_manager, "/MainMenu");

	gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 2);
	gtk_box_pack_end(GTK_BOX(vbox), main_frame, TRUE, TRUE, 2);

	gtk_signal_connect(GTK_OBJECT(main_win), "destroy",
			   GTK_SIGNAL_FUNC(on_main_window_destroy), NULL);

	gtk_widget_show_all(main_win);

	return 0;
}

int main(int argc, char *argv[])
{
	int err;
	struct maptool_state ms;

	gtk_init(&argc, &argv);
	g_thread_init(NULL);
	gdk_threads_init();
	err = create_maptool_ui(&ms);
	if (err)
		return -1;

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	return 0;
}
