#include <gtk/gtk.h>
#include "gropes-maptool-ui.h"

GtkWidget *frame;

void create_oikotie_ui(struct maptool_state *ms)
{
	frame = gtk_frame_new("Test oikotie");
	gtk_box_pack_start(GTK_BOX(ms->map_box), frame, TRUE, TRUE, 2);
	gtk_widget_show(frame);
}

void destroy_oikotie_ui(struct maptool_state *ms)
{
	gtk_widget_destroy(frame);
}
