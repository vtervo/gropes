#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>
#include "gropes.h"

struct gropes_state;
struct ui_info_area {
	GtkWidget *speed_frame;
	GtkWidget *speed_display;
	GtkWidget *location_frame;
	GtkWidget *location_display;
	GtkWidget *course_frame;
	GtkWidget *course_display;
	GtkWidget *pointer_loc_frame;
	GtkWidget *pointer_loc_display;
};

char *fmt_location(const struct gps_coord *coord);
gboolean on_darea_clicked(GtkWidget *widget,
			  GdkEventButton *event,
			  gpointer user_data);
gboolean on_pointer_motion(GtkWidget *widget,
			  GdkEventMotion *event,
			  gpointer user_data);
gboolean on_darea_expose(GtkWidget *widget,
			 GdkEventExpose *event,
			 gpointer user_data);
gboolean on_darea_configure(GtkWidget *widget,
			    GdkEventConfigure *event,
			    gpointer user_data);
void on_mode_change(GtkRadioAction *action, GtkRadioAction *current,
		    struct gropes_state *gs);
void on_fullscreen(GtkAction *action, struct gropes_state *gs);
void on_zoom_in(GtkAction *action, struct gropes_state *gs);
void on_zoom_out(GtkAction *action, struct gropes_state *gs);
void on_scroll(GtkAction *action, struct gropes_state *gs);
void on_gps_connect(GtkAction *action, struct gropes_state *gs);
void on_gps_disconnect(GtkAction *action, struct gropes_state *gs);
void on_gps_follow(GtkToggleAction *action, struct gropes_state *gs);
void on_menu_exit(GtkAction *action, struct gropes_state *gs);
GtkWidget *create_big_map_darea(struct gropes_state *state);
void on_main_window_destroy(GtkWidget *widget, gpointer data);

#endif /* UI_H */
