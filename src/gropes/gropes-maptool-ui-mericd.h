#include "gropes-maptool-ui.h"

#ifndef GROPES_MAPTOOL_UI_MERICD_H
#define GROPES_MAPTOOL_UI_MERICD_H

void create_mericd_ui(struct maptool_state *ms);
void destroy_mericd_ui(struct maptool_state *ms);

const struct maptool_provider mericd_prov = {
	.text		= "MeriCD",
	.create_ui 	= create_mericd_ui,
	.destroy_ui	= destroy_mericd_ui,
};
#endif
