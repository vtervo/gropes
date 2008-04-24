#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include <curl/curl.h>
#include <gpsnav/gpsnav.h>
#include <gpsnav/map.h>
#include <gpsnav/coord.h>

#include <lib_proj.h>

#define	SERVER		"http://kansalaisen.karttapaikka.fi"
#define SERVER_PATH	"/koordinaatit/koordinaatit.html"
#define COOKIE_FILE	"/tmp/karttapaikka.cookie"

/*---------------------------------------------------------------------------*/

#define MIN_LA	59.719733
#define MAX_LA	70.000000
#define MIN_LO	19.564125
#define MAX_LO	31.000000

#define CHECK_MIN(x, min)	{ if ((x) < (min)) \
						(x) = (min); }
#define CHECK_MAX(x, max)	{ if ((x) > (max)) \
					(x) = (max); }

struct kkj_zone {
	int		lo;	/* Center longitude of the zone */
	unsigned int	val;	/* Value of center longitude in meters */
};

/* Finnish KKJ zones based on the longitude */
static const struct kkj_zone kkj_zones[4] = {
	{ .lo	= 21,	.val	= 1500000 },
	{ .lo	= 24,	.val	= 2500000 },
	{ .lo	= 27,	.val	= 3500000 },
	{ .lo	= 30,	.val	= 4500000 }
};

/*
 * Returns the Finnish KKJ zone based on the Hayford longitude
 */
static struct kkj_zone * hayford_get_zone(double lo)
{
	int i;

	for (i = 0; i <= 3; i++) {
		double zone_lo = (double)kkj_zones[i].lo;

		if (lo <= (zone_lo + 1.5))
			return (struct kkj_zone *)&kkj_zones[i];
	}
	if (lo > 31.5 && lo < 31.601855)
		return (struct kkj_zone *)&kkj_zones[3];

	return NULL;
}

static struct kkj_zone * kkj_get_zone(double cx)
{
	int zone = (int)(cx / 1000000.0);

	switch (zone) {
	case 1:
		return (struct kkj_zone *)&kkj_zones[0];
	case 2:
		return (struct kkj_zone *)&kkj_zones[1];
	case 3:
		return (struct kkj_zone *)&kkj_zones[2];
	case 4:
		return (struct kkj_zone *)&kkj_zones[3];
	}

	return NULL;
}

static int wgs84_to_xy(struct gpsnav *nav,
		       XY *xy, const struct gps_coord *wgs84)
{
	const struct gps_datum *kkj;
	struct kkj_zone *zone;
	struct gps_coord hayf = *wgs84;
	char *projc[7];
	char lon_0[8];
	char x_0[11];
	PJ *pj;
	LP lp;

	kkj = gpsnav_find_datum(nav, "Finland Hayford");
	gpsnav_convert_datum(&hayf, NULL, kkj);

	/* REVISIT: Typically maps of whole Finland use zone 3 */
	zone = hayford_get_zone(hayf.lo);
	if (!zone) {
		fprintf(stderr, "Invalid WGS84 for KKJ zone: %.0f,%.0f\n",
			wgs84->la, wgs84->lo);
		return -1;
	}

	sprintf(x_0, "x_0=%07u", zone->val);
	sprintf(lon_0, "lon_0=%02i", zone->lo);
	projc[0] = "ellps=intl";
	projc[1] = "proj=tmerc";
	projc[2] = x_0;
	projc[3] = lon_0;
	pj = pj_init(4, projc);
	lp.phi = deg2rad(hayf.la);
	lp.lam = deg2rad(hayf.lo);
	*xy = pj_fwd(lp, pj);
	pj_free(pj);

	/* Only allow values within Finland  */
	if ((wgs84->la < MIN_LA) || (wgs84->la > MAX_LA) ||
	    (wgs84->lo < MIN_LO) || (wgs84->lo > MAX_LO)) {
		fprintf(stderr, "Invalid coordinate for KKJ %f,%f\n",
			wgs84->la, wgs84->lo);
		return -1;
	}

	return 0;
}

static int xy_to_wgs84(struct gpsnav *nav,
		       const XY *xy, struct gps_coord *position)
{
	const struct gps_datum *kkj;
	struct kkj_zone * zone;
	char *projc[7];
	char lon_0[8];
	char x_0[11];
	PJ *pj;
	LP lp;

	zone = kkj_get_zone(xy->x);
	if (!zone) {
		fprintf(stderr, "Invalid XY to convert to WGS84\n");
		return -1;
	}
	sprintf(x_0, "x_0=%07u", zone->val);
	sprintf(lon_0, "lon_0=%02i", zone->lo);
	kkj = gpsnav_find_datum(nav, "Finland Hayford");
	projc[0] = "ellps=intl";
	projc[1] = "proj=tmerc";
	projc[2] = x_0;
	projc[3] = lon_0;
	pj = pj_init(4, projc);
	lp = pj_inv(*xy, pj);
	position->lo = rad2deg(lp.lam);
	position->la = rad2deg(lp.phi);
	gpsnav_convert_datum(position, kkj, NULL);
	pj_free(pj);

	return 0;
}

/* Blacklist of areas with no map available to speed up downloading */
static struct gps_area nonmapped[] = {
	{
		/* Itäinen Suomenlahti */
		.start	= { .la = 59.719733, .lo = 28.512362 },
		.end	= { .la = 60.438113, .lo = 31.000000 }
	}, {
		.start	= { .la = 0.000000, .lo = 0.000000 },
		.end	= { .la = 0.000000, .lo = 0.000000 }
	}
};

/* Checks current coordinate is within margin of max and min coordinates */
static int gps_coord_within_bounds(const struct gps_coord *cur,
				   const struct gps_coord *min,
				   const struct gps_coord *max,
				   const double margin)
{
	if ((cur->lo >= min->lo) && (cur->la >= min->la) &&
	    (cur->lo <= max->lo) && (cur->la <= max->la))
		return 0;

	if (((min->la - cur->la) < margin) &&
	    ((min->lo - cur->lo) < margin) &&
	    ((cur->la - max->la) < margin) &&
	    ((cur->lo - max->lo) < margin))
		return 0;

	return -1;
}

static int gps_coord_nonmapped(const struct gps_coord *cur)
{
	struct gps_area *area = nonmapped;
	int ret;

	if (!cur)
		return 0;

	do {
		struct gps_coord *start, *end;

		start = &area->start;
		end = &area->end;

		if (start->la == 0)
			break;

		ret = gps_coord_within_bounds(cur, start, end, 0);
		if (ret == 0)
			return -1;

	} while (area++);

	return 0;
}

/* Aligns XY coordinate to map size */
static void align_xy(XY *xy, XY *increment_xy)
{
	unsigned int tmp;

	tmp = (int) xy->x;
	tmp /= increment_xy->x;
	tmp *= increment_xy->x;
	xy->x = tmp;

	tmp = (int) xy->y;
	tmp /= increment_xy->y;
	tmp *= increment_xy->y;
	xy->y = tmp;
}

/*---------------------------------------------------------------------------*/

struct karttapaikka_map {
	const char *name;
	int dim, extra_pix;		/* Image size in pixels */
	double scale;			/* Image scale = length / pixels */
	struct gps_marea marea;
};

static const struct karttapaikka_map kp_maps[] = {
	{
		.name  = "80000",
		.dim   = 600,
		.extra_pix = 0,
		.scale = 20,				/* 2.5km / 125 pixels */
		.marea = {
			.start.n	= 6623912,
			.start.e	= 1419370,
			.end.n		= 7780599,
			.end.e		= 5429669
		},
	}, {
		.name  = "40000",
		.dim   = 600,
		.extra_pix = 0,
		.scale = 10,				/* 1km / 100 pixels */
		.marea = {
			.start.n	= 6621972,
			.start.e	= 1421980,
			.end.n		= 7780599,
			.end.e		= 5429669
		},
	}, {
		.name  = "16000",
		.dim   = 600,
		.extra_pix = 0,
		.scale = 4,				/* 500m / 125 pixels */
		.marea = {
			.start.n	= 6621972,
			.start.e	= 1421980,
			.end.n		= 7780599,
			.end.e		= 5429669
		},
	},
};

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	FILE *f = userp;

	return fwrite(buffer, size, nmemb, f);
}

static int karttapaikka_add_map(struct gpsnav *nav,
				const struct karttapaikka_map *hmap,
				const char *fname,
				const struct gps_coord *start,
				const struct gps_coord *end,
				const struct kkj_zone *zone)
{
	struct gps_map *map;
	struct gps_key_value kv[7];
	char lon_0[8];
	char x_0[11];
	int r;

	map = gps_map_new();
	if (map == NULL)
		return -1;

	sprintf(x_0, "%07u", zone->val);
	sprintf(lon_0, "%02i", zone->lo);
	map->area.start = *start;
	map->area.end = *end;
	map->prov = gpsnav_find_provider(nav, "raster");
	kv[0].key = "bitmap-filename";
	kv[0].value = (char *) fname;
	kv[1].key = "bitmap-type";
	if (hmap->scale < 20)
		kv[1].value = "png";
	else
		kv[1].value = "jpeg";
	kv[2].key = "tag";
	kv[2].value = "karttapaikka";
	kv[3].key = "datum";
	kv[3].value = "Finland Hayford";
	kv[4].key = "projection";
	kv[4].value = "tmerc";
	kv[5].key = "central-meridian";
	kv[5].value = lon_0;
	kv[6].key = "false-easting";
	kv[6].value = x_0;

	r = gpsnav_add_map(nav, map, kv, 7, NULL);
	if (r < 0) {
		if (r == -EEXIST) {
			printf("Skipping duplicate map\n");
			r = 0;
		}
		free(map);
	}
	return r;
}

static int karttapaikka_download_map(CURL *curl,
				     const struct karttapaikka_map *hmap,
				     const unsigned int x, const unsigned int y,
				     const char *map_filename)
{
	char tmp_filename[512], url[512], *buf, *found;
	FILE *tmp_file, *map_file;
	CURLcode ccode;
	int ret, fd;

	curl_easy_reset(curl);
	sprintf(url, SERVER SERVER_PATH
		"?scale=%i&lang=FI&tool=siirra&cx=%u&cy=%u",
		4 * 1000 * (int)hmap->scale, x, y);
	strcpy(tmp_filename, map_filename);
	strcat(tmp_filename, ".html");

	/* Download the wrapper HTML page into a tmp file */
	tmp_file = fopen(tmp_filename, "w+");
	if (tmp_file == NULL) {
		perror(tmp_filename);
		return -1;
	}
	printf("URL %s\n", url);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, tmp_file);
	curl_easy_setopt(curl, CURLOPT_COOKIEFILE, COOKIE_FILE);
	ccode = curl_easy_perform(curl);
	fclose(tmp_file);
	if (ccode) {
		fprintf(stderr, "Unable to download map page '%s'\n",
			tmp_filename);
		return -1;
	}

	/* Parse the server generated map image file name */
	fd = open(tmp_filename, O_RDONLY);
	if (tmp_file == NULL) {
		perror(tmp_filename);
		return -1;
	}
	buf = malloc(10240);
	if (buf == NULL) {
		close(fd);
		fprintf(stderr, "Unable to allocate tmp buffer\n");
		return -1;
	}
	ret = read(fd, buf, 10240);
	if (ret < 1) {
		free(buf);
		close(fd);
		fprintf(stderr, "Unable to read tmp file ");
		perror(tmp_filename);
		return -1;
	}

	 /* Server generated image file name is something like
	  * /getMap?id=0123456789abcdef0123456789abcdef&lang=FI
	  */
	found = strstr(buf, "src=\"/image?request=GetMap");
	if (found == NULL) {
		free(buf);
		close(fd);
		fprintf(stderr, "Could not parse link to image file in %s\n",
			tmp_filename);
		return -1;
	}

	sprintf(url, SERVER);
	strncat(url, found + 5, 146);
	free(buf);
	close(fd);

	/* Download the map image file */
	map_file = fopen(map_filename, "w");
	if (map_file == NULL) {
		perror(map_filename);
		return -1;
	}

	printf("map URL %s\n", url);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, map_file);
	curl_easy_setopt(curl, CURLOPT_COOKIEFILE, COOKIE_FILE);

	ccode = curl_easy_perform(curl);
	fclose(map_file);
	if (ccode) {
		fprintf(stderr, "Unable to download map '%s'\n", map_filename);
		return -1;
	}

	ret = remove(tmp_filename);
	if (ret != 0) {
		fprintf(stderr, "Could not remove tmp file: ");
		perror(tmp_filename);
	}

	return 0;
}

static int karttapaikka_get_map(struct gpsnav *nav, CURL *curl,
				const struct karttapaikka_map *hmap,
				const XY *current_xy)
{
	char map_filename[512], *full_fname;
	struct gps_coord start, end;
	struct kkj_zone *zone;
	double center_offset;
	XY start_xy, end_xy;
	unsigned int x, y;
	struct stat st;
	int ret;

	x = (unsigned int) current_xy->x;
	y = (unsigned int) current_xy->y;

	sprintf(map_filename, "maps/map-karttapaikka-%s-%u-%u-%dx%d",
		hmap->name, y, x, hmap->dim, hmap->dim);
	if (hmap->scale < 20)
		strcat(map_filename, ".png");
	else
		strcat(map_filename, ".jpg");

	if (stat(map_filename, &st) != 0 || st.st_size == 0) {
		printf("Downloading to file '%s'...\n", map_filename);
		ret = karttapaikka_download_map(curl, hmap, x, y, map_filename);
		if (ret < 0)
			return -1;
	} else
		printf("Skipping existing file '%s'...\n", map_filename);

	full_fname = gpsnav_get_full_path(map_filename, NULL);
	zone = kkj_get_zone(current_xy->x);
	center_offset = ((int) hmap->scale * hmap->dim) / 2;
	start_xy.x = current_xy->x - center_offset;
	start_xy.y = current_xy->y - center_offset;
	end_xy.x = current_xy->x + center_offset;
	end_xy.y = current_xy->y + center_offset;

	if (xy_to_wgs84(nav, &start_xy, &start) != 0) {
		fprintf(stderr, "Bad start: %.0f,%.0f\n",
			start_xy.y, start_xy.x);
		return -1;
	}
	if (xy_to_wgs84(nav, &end_xy, &end) != 0) {
		fprintf(stderr, "Bad end: %.0f,%.0f\n",
			end_xy.y, end_xy.x);
		return -1;
	}

	ret = karttapaikka_add_map(nav, hmap, full_fname, &start, &end, zone);

	return ret;
}

static void karttapaikka_print_usage(void)
{
	int i;

	fprintf(stderr, "Specify map scale:\n");
        for (i = 0; i < sizeof(kp_maps)/sizeof(kp_maps[0]); i++)
		fprintf(stderr, "\t%s\n", kp_maps[i].name);
}


/*
 * Karttapaikka map scales 16000 - 80000 use KKJ zones 1 - 4.
 * Map scales 200000 - 12000000 use KKJ zone 3.
 */
int karttapaikka_dl(struct gpsnav *nav, struct gps_coord *start,
	       struct gps_coord *end, const char *map_name)
{
	const struct karttapaikka_map *hmap = NULL;
	CURL *curl;
	XY start_xy, end_xy, increment_xy, current_xy;
	int i, ret;

	curl = curl_easy_init();
	if (curl == NULL) {
		fprintf(stderr, "Unable to init curl\n");
		ret = -1;
		goto out;
	}
	if (map_name == NULL) {
		karttapaikka_print_usage();
		ret = -1;
		goto out;
	}
	for (i = 0; i < sizeof(kp_maps)/sizeof(kp_maps[0]); i++) {
		if (strcmp(map_name, kp_maps[i].name) == 0) {
			hmap = &kp_maps[i];
			break;
		}
	}
	if (hmap == NULL) {
		karttapaikka_print_usage();
		ret = -1;
		goto out;
	}

	/* Make sure the coordinates are within Finland */
	CHECK_MIN(start->la, MIN_LA);
	CHECK_MIN(start->lo, MIN_LO);
	CHECK_MAX(start->la, MAX_LA);
	CHECK_MAX(start->lo, MAX_LO);

	if ((end->la == 0) || (end->la < start->la))
		end->la = start->la;
	else {
		CHECK_MIN(end->la, MIN_LA);
		CHECK_MAX(end->la, MAX_LA);
	}

	if ((end->lo == 0) || (end->lo < start->lo))
		end->lo = start->lo;
	else {
		CHECK_MIN(end->lo, MIN_LO);
		CHECK_MAX(end->lo, MAX_LO);
	}

	ret = wgs84_to_xy(nav, &start_xy, start);
	if (ret != 0) {
		fprintf(stderr, "Invalid start coordinate\n");
		goto out;
	}
	ret = wgs84_to_xy(nav, &end_xy, end);
	if (ret != 0) {
		fprintf(stderr, "Invalid end coordinate\n");
		goto out;
	}

	/* Make sure the coordinates are within map area */
	CHECK_MIN(start_xy.x, hmap->marea.start.e);
	CHECK_MIN(start_xy.y, hmap->marea.start.n);
	CHECK_MAX(start_xy.x, hmap->marea.end.e);
	CHECK_MAX(start_xy.y, hmap->marea.end.n);

	CHECK_MIN(end_xy.x, hmap->marea.start.e);
	CHECK_MIN(end_xy.y, hmap->marea.start.n);
	CHECK_MAX(end_xy.x, hmap->marea.end.e);
	CHECK_MAX(end_xy.y, hmap->marea.end.n);

	increment_xy.x = hmap->dim * hmap->scale;
	increment_xy.y = hmap->dim * hmap->scale;

	start_xy.x = floor(start_xy.x);
	start_xy.y = floor(start_xy.y);
	align_xy(&start_xy, &increment_xy);

	end_xy.x = ceil(end_xy.x + increment_xy.x);
	end_xy.y = ceil(end_xy.y + increment_xy.y);
	align_xy(&end_xy, &increment_xy);

	printf("Requested map range %f,%f - %f,%f\n"
	       "Rounded map range %.0f,%.0f - %.0f,%.0f\n",
	       start->la, start->lo, end->la, end->lo,
	       start_xy.y, start_xy.x, end_xy.y, end_xy.x);

	for (current_xy.y = start_xy.y; current_xy.y <= end_xy.y;
	     current_xy.y += increment_xy.y) {

		for (current_xy.x = start_xy.x; current_xy.x <= end_xy.x;
		      current_xy.x += increment_xy.x) {
			struct gps_coord wgs84;

			if (xy_to_wgs84(nav, &current_xy, &wgs84) != 0)
				goto out;

			/* Fast forward to first longitude within the region */
			if ((current_xy.x == start_xy.x) &&
			    (wgs84.lo < start->lo))
				continue;

			ret = gps_coord_within_bounds(&wgs84, start, end, 0.1);
			if (ret != 0)
				break;

			ret = gps_coord_nonmapped(&wgs84);
			if (ret != 0)
				continue;

			printf("Getting map for %.0f,%.0f (%f,%f)\n",
			       current_xy.y, current_xy.x, wgs84.la, wgs84.lo);

			ret = karttapaikka_get_map(nav, curl, hmap, &current_xy);
			if (ret != 0) {
				fprintf(stderr, "Could not download map\n");
				goto out;
			}
		}
		ret = 0;
	}

out:
	curl_easy_cleanup(curl);

	return ret;
}
