#include <gtk/gtk.h>

#ifndef GROPES_MAPTOOL_UI_H
#define GROPES_MAPTOOL_UI_H

struct maptool_state {
	GtkWidget *main_win;
	struct gpsnav *nav;
	gchar *mapdb_file;
};

#endif
