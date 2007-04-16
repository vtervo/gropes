#include <gtk/gtk.h>
#include <gpsnav/gpsnav.h>

#include "gropes-maptool-ui.h"

GtkWidget *path, *path_label, *get_maps, *browse_folder;

static void on_browse_folder(GtkButton *button, struct maptool_state *ms)
{
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new ("Select MeriCD folder",
					      GTK_WINDOW(ms->main_win),
					      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					      NULL);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gtk_entry_set_text(GTK_ENTRY(path), filename);
		g_free (filename);
	}

	gtk_widget_destroy (dialog);
}

static void on_get_maps(GtkButton *button, struct maptool_state *ms)
{
	const char *map_path;

	map_path = gtk_entry_get_text(GTK_ENTRY(path));
	mericd_scan_directory(ms->nav, map_path);
}

void create_mericd_ui(struct maptool_state *ms)
{
	path_label = gtk_label_new("Meri CD location");
	path = gtk_entry_new();
	browse_folder = gtk_button_new_with_label("Browse flder..");
	get_maps = gtk_button_new_with_label("Get maps");
	gtk_box_pack_start(GTK_BOX(ms->map_box), path_label, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(ms->map_box), path, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(ms->map_box), browse_folder, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(ms->map_box), get_maps, TRUE, TRUE, 2);

	gtk_signal_connect(GTK_OBJECT(browse_folder), "clicked", G_CALLBACK(on_browse_folder), ms);
	gtk_signal_connect(GTK_OBJECT(get_maps), "clicked", G_CALLBACK(on_get_maps), ms);

	gtk_widget_show_all(ms->map_box);
}

void destroy_mericd_ui(struct maptool_state *ms)
{
	gtk_widget_destroy(path_label);
	gtk_widget_destroy(path);
	gtk_widget_destroy(get_maps);
	gtk_widget_destroy(browse_folder);
}
