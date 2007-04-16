#include "gropes-maptool-ui.h"

#ifndef GROPES_MAPTOOL_UI_OIKOTIE_H
#define GROPES_MAPTOOL_UI_OIKOTIE_H

void create_oikotie_ui(struct maptool_state *ms);
void destroy_oikotie_ui(struct maptool_state *ms);

const struct maptool_provider oikotie_prov = {
	.text		= "Oikotie",
	.create_ui	= create_oikotie_ui,
	.destroy_ui	= destroy_oikotie_ui,
};

#endif
