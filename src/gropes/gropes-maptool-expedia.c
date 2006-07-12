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

#define ALTI_TO_MPP	(2 * 1.4017295)

static const int map_width = 1000;
static const int map_height = 1000;
static const int x_extra = 2, y_extra = 2;

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	FILE *f = userp;

	return fwrite(buffer, size, nmemb, f);
}

static int expedia_add_map(struct gpsnav *nav, const char *fname,
			   const struct gps_coord *start,
			   const struct gps_coord *end)
{
	struct gps_map *map;
	struct gps_key_value kv[5];
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
	kv[1].value = "gif";
	kv[2].key = "tag";
	kv[2].value = "expedia";
	kv[3].key = "datum";
	kv[3].value = "WGS 84";
	kv[4].key = "projection";
	kv[4].value = "merc";
	r = gpsnav_add_map(nav, map, kv, 5, NULL);
	if (r < 0) {
		if (r == -EEXIST) {
			printf("Skipping duplicate map\n");
			r = 0;
		}
		free(map);
	}
	return r;
}

static int expedia_download_map(CURL *curl, int alti,
				const struct gps_coord *center, int width,
				int height, const char *map_filename)
{
	FILE *map_file;
	CURLcode ccode;
	char url[512];

	curl_easy_reset(curl);
#if 0
	sprintf(url, "http://expedia.com/pub/agent.dll?qscr=mrdt&ID=3XNsF.&CenP=%.9lf,%.9lf&Lang=%s&Alti=%d&Size=%d,%d&Offs=0.000000,0.000000&BCheck&tpid=1",
		center->la, center->lo, (center->lo > -30) ? "EUR0809" : "USA0409",
		alti, width, height);
#endif
	sprintf(url, "http://www.expedia.com/pub/agent.dll?qscr=mrdt&ID=3XNsF.&CenP=%.9lf,%.9lf&Lang=%s&Alti=%d&Size=%d,%d&Offs=0.000000,0.000000&Pins=|35eb|",
		center->la, center->lo, (center->lo > -30) ? "EUR0809" : "USA0409",
		alti, width, height);

	printf("%s\n", url);
	printf("Downloading '%s'...\n", map_filename);
	map_file = fopen(map_filename, "w");
	if (map_file == NULL) {
		perror(map_filename);
		return -1;
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, -1);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, map_file);
	ccode = curl_easy_perform(curl);
	fclose(map_file);
	if (ccode) {
		fprintf(stderr, "Unable to download map '%s'\nCurl error %s\n", map_filename, curl_easy_strerror(ccode));
		return -1;
	}

	return 0;
}

static int expedia_get_map(struct gpsnav *nav, CURL *curl, PJ *pj,
			   int alti, const struct gps_mcoord *mstart,
			   int map_width, int x_extra,
			   int map_height, int y_extra)
{
	char map_filename[512], *full_fname;
	struct gps_coord start, end, cent;
	struct stat st;
	double scale;
	LP lp;
	XY xy_start, xy_end, xy_cent;
	int r;

	sprintf(map_filename, "maps/map-expedia-%d-%d-%d-%dx%d.gif",
		alti, (int) mstart->n, (int) mstart->e,
		map_width, map_height);
	scale = alti * ALTI_TO_MPP;

	xy_cent.x = mstart->e + map_width * scale / 2;
	xy_cent.y = mstart->n + map_height * scale / 2;

	xy_cent.x += (x_extra / 2) * scale;
	xy_cent.y += (y_extra / 2) * scale;

	lp = pj_inv(xy_cent, pj);
	cent.lo = rad2deg(lp.lam);
	cent.la = rad2deg(lp.phi);

	if (stat(map_filename, &st) != 0 || st.st_size == 0) {
		r = expedia_download_map(curl, alti, &cent, map_width + x_extra,
					 map_height + y_extra, map_filename);
		if (r < 0)
			return -1;
	} else
		printf("Skipping existing file '%s'...\n", map_filename);

	xy_start.x = mstart->e;
	xy_start.y = mstart->n;
	xy_end.x = xy_start.x + (map_width + x_extra) * scale;
	xy_end.y = xy_start.y + (map_height + y_extra) * scale;

	lp = pj_inv(xy_start, pj);
	start.lo = rad2deg(lp.lam);
	start.la = rad2deg(lp.phi);

	lp = pj_inv(xy_end, pj);
	end.lo = rad2deg(lp.lam);
	end.la = rad2deg(lp.phi);

	full_fname = gpsnav_get_full_path(map_filename, NULL);
	r = expedia_add_map(nav, full_fname, &start, &end);
	free(full_fname);

	return r;
}

static void expedia_print_usage(void)
{
	fprintf(stderr, "Specify scale.\n");
}

int expedia_dl(struct gpsnav *nav, struct gps_coord *start,
	       struct gps_coord *end, const char *scale_str)
{
	struct gps_mcoord cur;
	double scale;
	char *projc[7];
	CURL *curl;
	XY start_xy, end_xy;
	int r, alti;
	PJ *pj;
	LP lp;

	curl = curl_easy_init();
	if (curl == NULL) {
		fprintf(stderr, "Unable to init curl\n");
		return -1;
	}

	if (scale_str == NULL || sscanf(scale_str, "%lf", &scale) != 1) {
		expedia_print_usage();
		return -1;
	}

	projc[0] = "ellps=WGS84";
	projc[1] = "proj=merc";
	pj = pj_init(2, projc);
	if (pj == NULL) {
		fprintf(stderr, "Unable to init projection\n");
		return -1;
	}

	lp.phi = deg2rad(start->la);
	lp.lam = deg2rad(start->lo);
	start_xy = pj_fwd(lp, pj);
	start_xy.x = floor(start_xy.x);
	start_xy.y = floor(start_xy.y);

	alti = rint(scale / ALTI_TO_MPP);
	if (alti == 0) {
		fprintf(stderr, "Scale too small (min. %f)\n", 1 * ALTI_TO_MPP);
		return -1;
	}
	scale = alti * ALTI_TO_MPP;
	printf("Using a scale of %f (alti %d)\n", scale, alti);

	start_xy.x -= (int) start_xy.x % (int) (map_width * scale);
	start_xy.y -= (int) start_xy.y % (int) (map_height * scale);

	lp.phi = deg2rad(end->la);
	lp.lam = deg2rad(end->lo);
	end_xy = pj_fwd(lp, pj);
	end_xy.x = ceil(end_xy.x);
	end_xy.y = ceil(end_xy.y);

	cur.n = start_xy.y;
	r = 0;
	do {
		double y_len;

		y_len = map_height * scale;
		cur.e = start_xy.x;
		do {
			double x_len;

			x_len = map_width * scale;
			if (expedia_get_map(nav, curl, pj, alti, &cur,
					    rint(x_len / scale), x_extra,
					    rint(y_len / scale), y_extra) < 0) {
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
