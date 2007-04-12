#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gpsnav/gpsnav.h>

#include "gropes-maptool-ui.h"

static void maptool_finalize(struct maptool_state *ms)
{
	if(ms->nav)
		gpsnav_finish(ms->nav);
}

static void on_main_window_destroy(GtkWidget *widget, struct maptool_state *ms)
{
	maptool_finalize(ms);
	gtk_main_quit();
}

static void on_menu_exit(GtkAction *action, struct maptool_state *ms)
{
	maptool_finalize(ms);
	gtk_main_quit();
}

static void on_menu_new(GtkAction *action, struct maptool_state *ms)
{
	GtkWidget *dialog;
	int r;

	if (ms->nav)
		gpsnav_finish(ms->nav);

	g_free(ms->mapdb_file);
	ms->mapdb_file = NULL;

	r = gpsnav_init(&ms->nav);
	if (r < 0) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(ms->main_win),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						"Error %d creating new database", r);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
}

static void on_menu_open(GtkAction *action, struct maptool_state *ms)
{
	GtkWidget *dialog;
	char *mapdb_file;
	int r = 0;

	if (ms->nav)
		gpsnav_finish(ms->nav);

	g_free(ms->mapdb_file);
	ms->mapdb_file = NULL;

	r = gpsnav_init(&ms->nav);
	if (r < 0) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(ms->main_win),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						"Error %d creating new database", r);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		return;
	}

	dialog = gtk_file_chooser_dialog_new("Open File", GTK_WINDOW(ms->main_win),
					     GTK_FILE_CHOOSER_ACTION_OPEN,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					     GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					     NULL);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		mapdb_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		r = gpsnav_mapdb_read(ms->nav, mapdb_file);
		g_free(mapdb_file);
		ms->mapdb_file = NULL;
	}

	gtk_widget_destroy(dialog);

	if (r < 0) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(ms->main_win),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						"Error %d reading database", r);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		gpsnav_finish(ms->nav);
	}
}

static void on_menu_save_as(GtkAction *action, struct maptool_state *ms)
{
	GtkWidget *dialog;
	int r = 0;

	if (ms->nav == NULL) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(ms->main_win),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						"No database to save");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		return;
	}

	dialog = gtk_file_chooser_dialog_new("Save File",
			GTK_WINDOW(ms->main_win),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER (dialog), TRUE);

	if(ms->mapdb_file == NULL) {
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "mapdb.xml");
	}
	else
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), ms->mapdb_file);


	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		g_free(ms->mapdb_file);
		ms->mapdb_file = g_strdup(filename);
		g_free(filename);
		r = gpsnav_mapdb_write(ms->nav, ms->mapdb_file);
	}

	gtk_widget_destroy (dialog);

	if (r < 0) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(ms->main_win),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						"Error %d writing database", r);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
}

static void on_menu_save(GtkAction *action, struct maptool_state *ms)
{
	GtkWidget *dialog;
	int r;

	if (ms->mapdb_file == NULL) {
		on_menu_save_as(action, ms);
		return;
	}

	if (ms->nav == NULL) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(ms->main_win),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						"No database to save");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		return;
	}

	r = gpsnav_mapdb_write(ms->nav, ms->mapdb_file);
	if (r < 0) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(ms->main_win),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				"Error %d writing database", r);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
}

static const GtkActionEntry menu_entries[] = {
	{ "FileMenu",	NULL,			"_File" },
	{ "New",	GTK_STOCK_NEW,		"_New", "<control>N", "New file", G_CALLBACK(on_menu_new) },
	{ "Open",	GTK_STOCK_OPEN,		"_Open...", "<control>O", "Open file", G_CALLBACK(on_menu_open) },
	{ "Save",	GTK_STOCK_SAVE,		"_Save", "<control>S", "Save file", G_CALLBACK(on_menu_save) },
	{ "Save_as",	GTK_STOCK_SAVE_AS,	"Save _As...", "<control><shift>S", "Save file", G_CALLBACK(on_menu_save_as) },
	{ "Exit",	GTK_STOCK_QUIT,		"E_xit", "<control>X", "Exit maptool", G_CALLBACK(on_menu_exit) },
};

static const char *ui_description =
"<ui>"
"  <menubar name='MainMenu'>"
"    <menu action='FileMenu'>"
"      <menuitem action='New'/>"
"      <menuitem action='Open'/>"
"      <menuitem action='Save'/>"
"      <menuitem action='Save_as'/>"
"      <menuitem action='Exit'/>"
"    </menu>"
"  </menubar>"
"</ui>";

static int create_maptool_ui(struct maptool_state *ms)
{
	GtkWidget *menu_bar, *vbox, *main_frame;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GError *error;

	ms->main_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(ms->main_win), "Gropes maptool");
	vbox = gtk_vbox_new(FALSE, 0);
	main_frame = gtk_frame_new("Frame");
	gtk_container_add(GTK_CONTAINER(ms->main_win), vbox);

	action_group = gtk_action_group_new("MenuActions");
	gtk_action_group_add_actions(action_group, menu_entries,
				     G_N_ELEMENTS(menu_entries), ms);

	ui_manager = gtk_ui_manager_new();
	accel_group = gtk_ui_manager_get_accel_group(ui_manager);
	gtk_window_add_accel_group(GTK_WINDOW(ms->main_win), accel_group);
	gtk_ui_manager_insert_action_group(ui_manager, action_group, 0);
	if (!gtk_ui_manager_add_ui_from_string(ui_manager, ui_description, -1, &error)) {
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
		return -1;
	}

	menu_bar = gtk_ui_manager_get_widget(ui_manager, "/MainMenu");

	gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 2);
	gtk_box_pack_end(GTK_BOX(vbox), main_frame, TRUE, TRUE, 2);

	gtk_signal_connect(GTK_OBJECT(ms->main_win), "destroy",
			   GTK_SIGNAL_FUNC(on_main_window_destroy), NULL);

	gtk_widget_show_all(ms->main_win);

	return 0;
}

int main(int argc, char *argv[])
{
	int err;
	struct maptool_state ms;

	memset(&ms, 0, sizeof(ms));
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
