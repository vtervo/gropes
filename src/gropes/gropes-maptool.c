#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <gpsnav/gpsnav.h>
#include <gpsnav/map.h>
#include <gpsnav/coord.h>
#include <math.h>

static const char *app_name = "gropes-maptool";

enum {
	OPT_ADD_MERICD_MAPS = 0x100,
	OPT_DL_KARTTA_HEL_MAPS,
	OPT_DL_OIKOTIE_MAPS,
	OPT_DL_EXPEDIA_MAPS,
	OPT_DL_KARTTAPAIKKA_MAPS
};

enum {
	CMD_ADD_MERICD_MAPS = 0,
	CMD_DL_KARTTA_HEL_MAPS,
	CMD_DL_OIKOTIE_MAPS,
	CMD_DL_EXPEDIA_MAPS,
	CMD_DL_KARTTAPAIKKA_MAPS
};

static const struct option options[] = {
	{ "add-mericd-maps",    1, 0, OPT_ADD_MERICD_MAPS },
	{ "download-hel-maps",	2, 0, OPT_DL_KARTTA_HEL_MAPS },
	{ "download-oikotie-maps",2,0,OPT_DL_OIKOTIE_MAPS },
	{ "download-expedia-maps",2,0,OPT_DL_EXPEDIA_MAPS },
	{ "download-karttapaikka-maps",2,0,OPT_DL_KARTTAPAIKKA_MAPS },
	{ "map-db-file",	1, 0, 'f' },
	{ "overwrite",		0, 0, 'o' },
	{ "start-coord",	1, 0, 's' },
	{ "end-coord",		1, 0, 'e' },
	{ NULL },
};

static const char *option_str = "f:os:e:?";

static const char *option_help[] = {
	"Add MeriCD maps from specified directory",
	"Download maps from kartta.hel.fi",
        "Download maps from Oikotie",
	"Download maps from expedia.com",
	"Download maps from kansalaisen.karttapaikka.com",
	"Map database filename",
	"Overwrite existing map database",
	"Start (bottom left) coordinate",
	"End (top right) coordinate",
	NULL
};

static void print_usage_and_die(void)
{
	int i = 0;
	printf("Usage: %s [OPTIONS]\nOptions:\n", app_name);

	while (options[i].name) {
		char buf[40], tmp[5];
		const char *arg_str;

		/* Skip "hidden" options */
		if (option_help[i] == NULL) {
			i++;
			continue;
		}

		if (options[i].val > 0 && options[i].val < 128)
			sprintf(tmp, ", -%c", options[i].val);
		else
			tmp[0] = 0;
		switch (options[i].has_arg) {
		case 1:
			arg_str = " <arg>";
			break;
		case 2:
			arg_str = " [arg]";
			break;
		default:
			arg_str = "";
			break;
		}
		sprintf(buf, "--%s%s%s", options[i].name, tmp, arg_str);
		if (strlen(buf) > 29) {
			printf("  %s\n", buf);
			buf[0] = '\0';
		}
		printf("  %-29s %s\n", buf, option_help[i]);
		i++;
	}
	exit(2);
}

extern int kartta_hel_dl(struct gpsnav *nav, struct gps_coord *start,
			 struct gps_coord *end, const char *map);
extern int oikotie_dl(struct gpsnav *nav, struct gps_coord *start,
		      struct gps_coord *end, const char *map);
extern int expedia_dl(struct gpsnav *nav, struct gps_coord *start,
		      struct gps_coord *end, const char *scale);
extern int karttapaikka_dl(struct gpsnav *nav, struct gps_coord *start,
		      struct gps_coord *end, const char *scale);

static const char *mapdb_file = "mapdb.xml";

int main(int argc, char *argv[])
{
	int c, long_optind, r;
	unsigned long flags = 0;
	const char *mericd_dir = NULL;
	const char *hel_map = NULL;
	const char *oikotie_map = NULL;
	const char *expedia_scale = NULL;
	const char *karttapaikka_map = NULL;
	int overwrite = 0;
	struct gps_coord start, end;
	struct gpsnav *nav;

	memset(&start, 0, sizeof(start));
	memset(&end, 0, sizeof(end));
	for (;;) {
		c = getopt_long(argc, argv, option_str, options, &long_optind);
		if (c == -1)
			break;
		if (c == '?')
			print_usage_and_die();
		switch (c) {
		case OPT_ADD_MERICD_MAPS:
			flags |= (1 << CMD_ADD_MERICD_MAPS);
			mericd_dir = optarg;
			break;
		case OPT_DL_KARTTA_HEL_MAPS:
			flags |= (1 << CMD_DL_KARTTA_HEL_MAPS);
			hel_map = optarg;
			break;
		case OPT_DL_OIKOTIE_MAPS:
			flags |= (1 << CMD_DL_OIKOTIE_MAPS);
			oikotie_map = optarg;
			break;
		case OPT_DL_EXPEDIA_MAPS:
			flags |= (1 << CMD_DL_EXPEDIA_MAPS);
			expedia_scale = optarg;
			break;
		case OPT_DL_KARTTAPAIKKA_MAPS:
			flags |= (1 << CMD_DL_KARTTAPAIKKA_MAPS);
			karttapaikka_map = optarg;
			break;
		case 'f':
			mapdb_file = optarg;
			break;
		case 's':
			if (sscanf(optarg, "%lf,%lf", &start.la, &start.lo) != 2) {
				fprintf(stderr, "Format has to be 'y.yyyyyy,x.xxxxxx'\n");
				return 2;
			}
			break;
		case 'e':
			if (sscanf(optarg, "%lf,%lf", &end.la, &end.lo) != 2) {
				fprintf(stderr, "Format has to be 'y.yyyyyy,x.xxxxxx'\n");
				return 2;
			}
			break;
		case 'o':
			overwrite = 1;
			break;
		}
	}
	if (!flags) {
		fprintf(stderr, "Nothing to do!\n");
		print_usage_and_die();
	}

	r = gpsnav_init(&nav);
	if (r < 0) {
		fprintf(stderr, "gpsnav initialization failed\n");
		return 1;
	}

	if (!overwrite && access(mapdb_file, R_OK) == 0)
		gpsnav_mapdb_read(nav, mapdb_file);

	if (flags & (1 << CMD_ADD_MERICD_MAPS)) {
		r = mericd_scan_directory(nav, mericd_dir);
		if (r < 0)
			goto err;
	}

	if (flags & (1 << CMD_DL_KARTTA_HEL_MAPS)) {
		if (kartta_hel_dl(nav, &start, &end, hel_map) < 0)
			goto err;
	}

	if (flags & (1 << CMD_DL_OIKOTIE_MAPS)) {
		if (oikotie_dl(nav, &start, &end, oikotie_map) < 0)
			goto err;
	}

	if (flags & (1 << CMD_DL_EXPEDIA_MAPS)) {
		if (expedia_dl(nav, &start, &end, expedia_scale) < 0)
			goto err;
	}
	if (flags & (1 << CMD_DL_KARTTAPAIKKA_MAPS)) {
		if (karttapaikka_dl(nav, &start, &end, karttapaikka_map) < 0)
			goto err;
	}

	printf("Writing map database to '%s'...\n", mapdb_file);
	gpsnav_mapdb_write(nav, mapdb_file);

	gpsnav_finish(nav);
	return 0;
err:
	gpsnav_finish(nav);
	return 2;
}
