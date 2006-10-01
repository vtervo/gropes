#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include <curl/curl.h>
#include <gpsnav/gpsnav.h>
#include <gpsnav/map.h>
#include <gpsnav/coord.h>

#include <lib_proj.h>

struct hel_map {
	const char *name;
	const char *url;
	int dim, extra_pix;
	double scale;
	struct gps_marea marea;
	double width, height;
};

static const struct hel_map big_map = {
	.name  = "big",
	.url   = "/KARTAT/OPAS_PKS/pks_opas2.pss",
	.dim = 1000,
	.extra_pix = 2,
	.scale = 2.0,
	.marea = {
		.start.n = 6665000,
		.start.e = 2526000,
		.end.n   = 6700000,
		.end.e   = 2566000
	},
	.width = 35000,
	.height = 35000,
};

static const struct hel_map small_map = {
	.name  = "small",
	.url   = "/KARTAT/YLEISKARTAT/YLEISKARTTA_PIENI.iTIFF",
	.dim = 1500,
	.extra_pix = 0,
	.scale = 28.0,
	.marea = {
		.start.n = 6667258,
		.start.e = 2527780,
		.end.n   = 6700000,
		.end.e   = 2568050
	},
	.width = 32742,
        .height = 32742,
};

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	FILE *f = userp;

	return fwrite(buffer, size, nmemb, f);
}

static int hel_kartta_add_map(struct gpsnav *nav, const char *fname,
			      const struct gps_coord *start,
			      const struct gps_coord *end)
{
	struct gps_map *map;
	struct gps_key_value kv[7];
	int r;

	map = gps_map_new();
	if (map == NULL)
		return -1;
	map->area.start = *start;
	map->area.end = *end;
	map->prov = gpsnav_find_provider(nav, "raster");
	kv[0].key = "bitmap-filename";
	kv[0].value = (char *) fname;
	kv[1].key = "bitmap-type";
	kv[1].value = "png";
	kv[2].key = "tag";
	kv[2].value = "kartta.hel.fi";
	kv[3].key = "datum";
	kv[3].value = "Finland Hayford";
	kv[4].key = "projection";
	kv[4].value = "tmerc";
	kv[5].key = "central-meridian";
	kv[5].value = "24";
	kv[6].key = "false-easting";
	kv[6].value = "2500000";
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

static int hel_kartta_download_map(CURL *curl, const struct hel_map *hmap,
				   const struct gps_mcoord *top_left, int width,
				   int height, const char *map_filename)
{
	FILE *map_file;
	CURLcode ccode;
	char url[512];
	const struct gps_marea *map_area = &hmap->marea;
	double map_width, map_height;
	double x_per, y_per, width_per, height_per;

	map_width = hmap->width;
	map_height = hmap->height;

	x_per = (top_left->e - map_area->start.e) / map_width;
	y_per = (map_area->start.n + map_height - top_left->n) / map_height;
	width_per = width * hmap->scale / map_width;
	height_per = height * hmap->scale / map_height;

	curl_easy_reset(curl);
	sprintf(url, "http://kartta2.hel.fi/?fif=%s&obj=hip,1.3&wid=%d&hei=%d&Fit=5&RGN=%.12f,%.12f,%.12E,%.12E&cvt=png",
		hmap->url, width, height, x_per, y_per, width_per, height_per);
	printf("Downloading '%s'...\n", map_filename);
	map_file = fopen(map_filename, "w");
	if (map_file == NULL) {
		perror(map_filename);
		return -1;
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, map_file);
	ccode = curl_easy_perform(curl);
	fclose(map_file);
	if (ccode) {
		fprintf(stderr, "Unable to download map '%s'\n", map_filename);
		return -1;
	}

	return 0;
}

static int hel_kartta_get_map(struct gpsnav *nav, CURL *curl, PJ *pj,
			      const struct hel_map *hmap,
			      const struct gps_mcoord *mstart,
			      int map_width, int x_extra,
			      int map_height, int y_extra)
{
	char map_filename[512], *full_fname;
	const struct gps_datum *kkj;
	struct gps_coord start, end;
	struct gps_mcoord mtopleft;
	struct stat st;
	LP lp;
	XY xy_start, xy_end;
	int r;

	sprintf(map_filename, "maps/map-hel-%s-%d-%d-%dx%d.png",
		hmap->name, (int) mstart->n, (int) mstart->e,
		map_width, map_height);

	mtopleft.e = mstart->e;
	mtopleft.n = mstart->n + map_height * hmap->scale;

	mtopleft.e -= (x_extra / 2) * hmap->scale;
	mtopleft.n += (y_extra / 2) * hmap->scale;

	if (stat(map_filename, &st) != 0 || st.st_size == 0) {
		r = hel_kartta_download_map(curl, hmap, &mtopleft, map_width + x_extra,
					    map_height + y_extra, map_filename);
		if (r < 0)
			return -1;
	} else
		printf("Skipping existing file '%s'...\n", map_filename);

	xy_start.x = mtopleft.e;
	xy_end.y = mtopleft.n;
	xy_start.y = xy_end.y - (map_height + y_extra) * hmap->scale;
	xy_end.x = xy_start.x + (map_width + x_extra) * hmap->scale;
#if 0
	/* Adjust the northing of start and end coordinates according
	 * to empirical observations */
	xy_start.y += 2.0;
	xy_end.y += 2.0;
#endif

	kkj = gpsnav_find_datum(nav, "Finland Hayford");
	if (kkj == NULL) {
		gps_error("Unable to find KKJ datum");
		return -1;
	}

	lp = pj_inv(xy_start, pj);
	start.lo = rad2deg(lp.lam);
	start.la = rad2deg(lp.phi);
	gpsnav_convert_datum(&start, kkj, NULL); /* from KKJ to WGS84 */

	lp = pj_inv(xy_end, pj);
	end.lo = rad2deg(lp.lam);
	end.la = rad2deg(lp.phi);
	gpsnav_convert_datum(&end, kkj, NULL);

	full_fname = gpsnav_get_full_path(map_filename, NULL);
	r = hel_kartta_add_map(nav, full_fname, &start, &end);
	free(full_fname);

	return r;
}

static void hel_kartta_print_usage(void)
{
	fprintf(stderr, "Specify map type: either '%s' or '%s'\n",
		big_map.name, small_map.name);
}

int kartta_hel_dl(struct gpsnav *nav, struct gps_coord *start,
		  struct gps_coord *end, const char *map_name)
{
	const struct hel_map *hmap;
	struct gps_coord start_hayf, end_hayf;
	const struct gps_datum *kkj;
	struct gps_mcoord cur;
	char *projc[7];
	CURL *curl;
	XY start_xy, end_xy;
	PJ *pj;
	LP lp;
	int r;

	curl = curl_easy_init();
	if (curl == NULL) {
		fprintf(stderr, "Unable to init curl\n");
		return -1;
	}

	if (map_name == NULL) {
		hel_kartta_print_usage();
		return -1;
	}
	if (strcmp(map_name, big_map.name) == 0)
		hmap = &big_map;
	else if (strcmp(map_name, small_map.name) == 0)
		hmap = &small_map;
	else {
		hel_kartta_print_usage();
		return -1;
	}

	kkj = gpsnav_find_datum(nav, "Finland Hayford");
	start_hayf = *start;
	gpsnav_convert_datum(&start_hayf, NULL, kkj);
	end_hayf = *end;
	gpsnav_convert_datum(&end_hayf, NULL, kkj);

	projc[0] = "ellps=intl";
	projc[1] = "proj=tmerc";
	/* KKJ zone 2 */
	projc[2] = "x_0=2500000";
	projc[3] = "lon_0=24";
	pj = pj_init(4, projc);

	lp.phi = deg2rad(start_hayf.la);
	lp.lam = deg2rad(start_hayf.lo);
	start_xy = pj_fwd(lp, pj);
	start_xy.x = floor(start_xy.x);
	start_xy.y = floor(start_xy.y);

	start_xy.x -= (int) start_xy.x % (int) (hmap->dim * hmap->scale);
	start_xy.y -= (int) start_xy.y % (int) (hmap->dim * hmap->scale);

	if (start_xy.x < hmap->marea.start.e) {
		printf("Start coordinate easting is too small\n");
		start_xy.x = hmap->marea.start.e;
	}
	if (start_xy.y < hmap->marea.start.n) {
		printf("Start coordinate northing is too small\n");
		start_xy.y = hmap->marea.start.n;
	}

	lp.phi = deg2rad(end_hayf.la);
	lp.lam = deg2rad(end_hayf.lo);
	end_xy = pj_fwd(lp, pj);
	end_xy.x = ceil(end_xy.x);
	end_xy.y = ceil(end_xy.y);

	if (end_xy.x > hmap->marea.end.e) {
		printf("End coordinate easting is too large\n");
		end_xy.x = hmap->marea.end.e;
	}
	if (end_xy.y > hmap->marea.end.n) {
		printf("End coordinate northing is too large\n");
		end_xy.y = hmap->marea.end.n;
	}

	cur.n = start_xy.y;
	r = 0;
	do {
		double y_len;

		y_len = hmap->dim * hmap->scale;
		if (y_len > hmap->marea.end.n - cur.n)
			y_len = hmap->marea.end.n - cur.n;
		else if ((int) cur.n % (int) y_len)
			y_len -= (int) cur.n % (int) y_len;
		cur.e = start_xy.x;
		do {
			double x_len;

			x_len = hmap->dim * hmap->scale;
			if (x_len > hmap->marea.end.e - cur.e)
				x_len = hmap->marea.end.e - cur.e;
			else if ((int) cur.e % (int) x_len)
				x_len -= (int) cur.e % (int) x_len;
			if (hel_kartta_get_map(nav, curl, pj, hmap, &cur,
					       x_len / hmap->scale, hmap->extra_pix,
					       y_len / hmap->scale, hmap->extra_pix) < 0) {
				r = -1;
				goto fail;
			}
			cur.e += x_len;
		} while (cur.e < end_xy.x);
		cur.n += y_len;
	} while (cur.n < end_xy.y);
fail:
	pj_free(pj);
	curl_easy_cleanup(curl);
	return r;
}
