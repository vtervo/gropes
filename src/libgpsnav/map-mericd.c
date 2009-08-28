#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <endian.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <lib_proj.h>
#include <gpsnav/gpsnav.h>
#include <gpsnav/map.h>
#include <gpsnav/coord.h>

#define GMB_HEADER_SIZE		 0x0fb2
#define GMB_WIDTH_OFFSET	 0x007e
#define GMB_HEIGHT_OFFSET	 0x0092
#define GMB_B_OFFSET		 0x00b5
#define GMB_N_COLORS_OFFSET	 0x00b0
#define GMB_N_USED_COLORS_OFFSET 0x00a6
#define GMB_OFFSET_TABLE_OFFSET	 0x26d7

struct mericd_data {
	PJ *proj;
};

struct gmb_map {
	unsigned short width;
	unsigned short height;
	unsigned short nr_colors;
	struct gmb_color {
		uint32_t pix32;
		uint16_t pix16;
	} palette[32];
	void *data;

	char *gmb_filename;
	size_t file_size;
};

static inline uint32_t le32_to_cpu(uint32_t x)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint32_t n;
	uint8_t *p = (uint8_t *) &x;

	n = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];

	return n;
#else
	return x;
#endif
}

static int decode_gmb_line(struct gmb_map *img,
			   unsigned char **gmb_data, int *gmb_data_size,
			   unsigned char **pixel_data,
			   int x_offset, int width, int bpp)
{
	unsigned char *p, *out;
	struct gmb_color *palette;
	int x, left;

	palette = img->palette;
	p = *gmb_data;
	out = *pixel_data;
	left = *gmb_data_size;
	x = 0;
	while (x < img->width) {
		uint8_t c;
		int count, color;
		uint32_t pix16, pix32;

		if (left < 1)
			return -2;
		c = *p++;
		left--;

		switch (img->nr_colors) {
		case 16:
			count = c & 0x0f;
			if (count == 0) {
				/* 16-bit length */
				if (left < 2)
					return -2;
				count = (*p++ << 8);
				count |= *p++;
				left -= 2;
			} else if (count & 0x08) {
				/* 11-bit length */
				if (left < 1)
					return -2;
				count = ((count & 0x07) << 8) + *p++;
				left--;
			} else {
				/* 3-bit length */
			}
			color = c >> 4;
			break;
		default:
			if (left < 1)
				return -2;
			count = *p++;
			left--;
			if (count & 0x80) {
				/* 16-bit length */
				if (left < 1)
					return -2;
				count = ((count & 0x7f) << 8) + *p++;
				left--;
			}
			color = c;
			break;
		}
		if (x < x_offset) {
			if (x + count < x_offset) {
				/* We discard all the pixels */
				x += count;
				continue;
			} else {
				count -= x_offset - x;
				x = x_offset;
			}
		} else if (x > x_offset + width) {
			/* We discard all the pixels */
			x += count;
			continue;
		}
		if (x + count > x_offset + width) {
			int new_x;

			new_x = x + count;
			count = x_offset + width - x;
			x = new_x;
		} else
			x += count;

		pix16 = palette[color].pix16;
		pix32 = palette[color].pix32;

		/* A little optimization here to write two 16-bit
		 * pixels at a time. Pretty fast on ARM. */
		if (bpp == 16 && ((unsigned long) out & 0x03) == 0) {
			uint32_t l;
			unsigned char *endp;

			l = (pix16 << 16) | pix16;
			endp = out + (count & ~1) * 2;
			while (out < endp) {
				*((unsigned int *) out) = l;
				out += 4;
			}
			count &= 1;
		}
		while (count--) {
			switch (bpp) {
			case 16:
				*((unsigned short *) out) = pix16;
				out += 2;
				break;
			case 24:
				*out++ = pix32 >> 16;
				*out++ = pix32 >> 8;
				*out++ = pix32;
				break;
			case 32:
				*((unsigned int *) out) = pix32;
				out += 4;
				break;
			}
		}
	}
	if (x > img->width) {
		fprintf(stderr, "ran over line length, aieee!\n");
		return -1;
	}
	*gmb_data = p;
	*pixel_data = out;
	*gmb_data_size = left;
	return 0;
}

static int decode_gmb_pixels(struct gmb_map *img,
			     unsigned char *out, int x, int y_start,
			     int width, int height, int bpp, int row_stride)
{
	unsigned int offset, offset_end;
	unsigned char *buf, *p;
	int n, fd, y;

	fd = open(img->gmb_filename, O_RDONLY);
	if (fd < 0)
		return -1;

	if (lseek(fd, GMB_OFFSET_TABLE_OFFSET + y_start * 4, SEEK_SET) < 0) {
		perror("lseek");
		fprintf(stderr, "was trying to seek to offset %u\n",
			GMB_OFFSET_TABLE_OFFSET + y_start * 4);
		goto fail;
	}
	if (read(fd, &offset, sizeof(offset)) != 4) {
		perror("read");
		goto fail;
	}
	if (y_start + height == img->height)
		offset_end = img->file_size;
	else {
		if (lseek(fd, GMB_OFFSET_TABLE_OFFSET + (y_start + height) * 4, SEEK_SET) < 0) {
			perror("lseek");
			fprintf(stderr, "was trying to lseek to offset %u\n",
				GMB_OFFSET_TABLE_OFFSET + (y_start + height) * 4);
			goto fail;
		}
		if (read(fd, &offset_end, sizeof(offset)) != 4) {
			perror("read");
			goto fail;
		}
		offset_end = le32_to_cpu(offset_end);
	}
	if (lseek(fd, offset, SEEK_SET) < 0) {
		perror("seek");
		goto fail;
	}
	n = offset_end - offset;
	buf = malloc(n);
	if (buf == NULL)
		goto fail;
	if (read(fd, buf, n) != n) {
		perror("error reading pixel data");
		goto fail2;
	}
	p = buf;
	row_stride -= width * (bpp / 8);
	for (y = y_start; y < y_start + height; y++) {
		if (decode_gmb_line(img, &p, &n, &out, x, width, bpp) < 0) {
			fprintf(stderr, "decoding of GMB line %d failed\n", y);
			goto fail2;
		}
		out += row_stride;
	}
	close(fd);
	free(buf);
	return 0;
fail2:
	free(buf);
fail:
	close(fd);
	return -1;
}

static int parse_gmb_header(struct gmb_map *img, char *hdr)
{
	int i;

	if (sscanf(hdr + GMB_WIDTH_OFFSET, "%hu", &img->width) != 1)
		return -1;
	if (sscanf(hdr + GMB_HEIGHT_OFFSET, "%hu", &img->height) != 1)
		return -1;
	if (sscanf(hdr + GMB_N_COLORS_OFFSET, "%hu", &img->nr_colors) != 1)
		return -1;
	if (img->nr_colors == 0) {
		if (sscanf(hdr + GMB_N_USED_COLORS_OFFSET, "%hu", &img->nr_colors) != 1)
			return -1;
	}
	if (img->nr_colors > sizeof(img->palette)/sizeof(img->palette[0])) {
		gps_error("%s: map uses too many colors", img->gmb_filename);
		return -1;
	}
	hdr += GMB_B_OFFSET;
	for (i = 0; i < img->nr_colors; i++) {
		unsigned short r, g, b;

		if (sscanf(hdr, "%hu %hu %hu", &r, &g, &b) != 3)
			return -1;
		img->palette[i].pix32 = (r << 16) | (g << 8) | b;
		r = r * 15 / 255;
		g = g * 31 / 255;
		b = b * 15 / 255;
		img->palette[i].pix16 = (r << 11) | (g << 5) | b;

		hdr += 15;
	}
	return 0;
}

static int parse_gmb(struct gmb_map *img, const char *filename)
{
	struct stat st;
	unsigned char hdr[GMB_HEADER_SIZE];
	FILE *f;

	f = fopen(filename, "r");
	if (f == NULL) {
		gps_error("Unable to open GMB file '%s': %s", filename,
			  strerror(errno));
		return -1;
	}
	if (fstat(fileno(f), &st) < 0)
		goto fail;
	img->file_size = st.st_size;
	if (fread(hdr, GMB_HEADER_SIZE, 1, f) != 1)
		goto fail;
	if (parse_gmb_header(img, (char *) hdr) < 0)
		goto fail;
//	printf("GMB header ok: %ux%u, %d colors\n",
//	       img->width, img->height, img->nr_colors);
	return 0;
fail:
	fclose(f);
	return -1;
}

static int mericd_get_pixels(struct gpsnav *nav, struct gps_map *map,
			     struct gps_pixel_buf *pb)
{
	struct gmb_map *gmb_map = map->data;
	int r;

	r = decode_gmb_pixels(gmb_map, pb->data, pb->x, pb->y, pb->width,
			      pb->height, pb->bpp, pb->row_stride);
	if (r < 0)
		return -1;
	return 0;
}

static int mericd_get_map_info(struct gpsnav *nav, struct gps_map *map,
			       struct gps_key_value **kv_out, int *kv_count,
			       const char *base_path)
{
	struct gmb_map *gmb_map = map->data;
	struct gps_key_value *kv;

	kv = malloc(sizeof(*kv) * 1);
	if (kv == NULL)
		return -ENOMEM;
	kv[0].key = strdup("gmb-filename");
	if (kv[0].key == NULL) {
		free(kv);
		return -ENOMEM;
	}
	kv[0].value = gpsnav_remove_base_path(gmb_map->gmb_filename, base_path);
	if (kv[0].value == NULL) {
		free(kv);
		free(kv[0].key);
		return -ENOMEM;
	}

	*kv_out = kv;
	*kv_count = 1;

	return 0;
}

static int mericd_init(struct gpsnav *gpsnav, struct gps_map_provider *prov)
{
	struct mericd_data *data;
	char *projc[3];
	PJ *pj;

	data = malloc(sizeof(*data));
	if (data == NULL)
		return -ENOMEM;
	memset(data, 0, sizeof(*data));

	projc[0] = "ellps=WGS84";
	projc[1] = "proj=merc";
	projc[2] = "no_defs";
	pj = pj_init(3, projc);
	if (pj == NULL) {
		gps_error("Unable to initialize projection");
		free(data);
		return -1;
	}
	data->proj = pj;
	prov->data = data;

	return 0;
}

static void mericd_finish(struct gpsnav *gpsnav, struct gps_map_provider *prov)
{
	struct mericd_data *data = prov->data;

	pj_free(data->proj);
	free(data);
}

static int mericd_check_for_dupe(struct gpsnav *nav, const char *fname)
{
	struct gps_map *e;

	for (e = nav->map_list.lh_first; e != NULL; e = e->entries.le_next) {
		struct gmb_map *m;

		if (strcmp(e->prov->name, "MeriCD") != 0)
			continue;
		m = e->data;
		if (strcmp(m->gmb_filename, fname) == 0)
			return -1;
	}
	return 0;
}

static int mericd_add_map(struct gpsnav *nav, struct gps_map *map,
			  struct gps_key_value *kv, int kv_count,
			  const char *base_path)
{
	struct gmb_map *gmb_map;
	struct mericd_data *data = map->prov->data;
	char *gmb_filename;
	int i, r;

	gmb_filename = NULL;
	for (i = 0; i < kv_count; i++) {
		if (strcmp(kv->key, "gmb-filename") == 0)
			gmb_filename = kv->value;
	}
	if (gmb_filename == NULL) {
		gps_error("Did not get GMB filename");
		return -1;
	}

	gmb_filename = gpsnav_get_full_path(gmb_filename, base_path);
	if (gmb_filename == NULL)
		return -ENOMEM;

	if (mericd_check_for_dupe(nav, gmb_filename) < 0) {
		free(gmb_filename);
		return -EEXIST;
	}
	gmb_map = malloc(sizeof(*gmb_map));
	if (gmb_map == NULL) {
		free(gmb_filename);
		return -1;
	}
	r = parse_gmb(gmb_map, gmb_filename);
	if (r < 0) {
		free(gmb_map);
		free(gmb_filename);
		return -1;
	}
	map->width = gmb_map->width;
	map->height = gmb_map->height;
	map->proj = data->proj;

	gmb_map->gmb_filename = gmb_filename;

	map->data = gmb_map;

	return 0;
}

static void mericd_free_map(struct gps_map *map)
{
	struct gmb_map *gmb_map = map->data;

	free(gmb_map->gmb_filename);
	free(gmb_map);
}

struct gps_map_provider mericd_provider = {
	.name = "MeriCD",
	.get_pixels = mericd_get_pixels,
	.add_map = mericd_add_map,
	.get_map_info = mericd_get_map_info,
	.free_map = mericd_free_map,
	.init = mericd_init,
	.finish = mericd_finish
};

static int find_gmb_file(FILE *f, const char *in8_fname, char *buf,
			 size_t buf_len)
{
	char line[256], *p;
	int found, len, i;
	struct stat st;

	found = 0;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strncasecmp(line, "[DATAFILE]", 10) == 0) {
			found = 1;
			break;
		}
	}
	if (found) {
		char tmp_fname[64];

		if (fgets(tmp_fname, sizeof(tmp_fname), f) == NULL)
			return -1;
		while (tmp_fname[strlen(tmp_fname) - 1] == '\r' ||
		       tmp_fname[strlen(tmp_fname) - 1] == '\n')
			tmp_fname[strlen(tmp_fname) - 1] = '\0';
		strcpy(line, in8_fname);
		p = strrchr(line, '/');
		if (p == NULL)
			p = line;
		else
			p++;
		strncpy(p, tmp_fname, sizeof(line) - (p - line) - 1);
		line[sizeof(line) - 1] = '\0';
	} else {
		if (strlen(in8_fname) >= sizeof(line))
			return -1;
		strcpy(line, in8_fname);
		p = strrchr(line, '.');
		if (p == NULL)
			return -1;
		strcpy(p, ".gmb");
	}

	len = strlen(line);
	if (len >= buf_len)
		return -1;

	strcpy(buf, line);

	if (stat(buf, &st) == 0)
		return 0;

	p = strrchr(buf, '/');
	if (p == NULL)
		p = buf;

	for (i = p - buf; i < len; i++)
		buf[i] = tolower(buf[i]);
	if (stat(buf, &st) == 0)
		return 0;

	for (i = p - buf; i < len; i++)
		buf[i] = toupper(buf[i]);
	if (stat(buf, &st) == 0)
		return 0;

	return -1;
}

static double parse_coord_value(double in)
{
	double deg, min;

	deg = (int) in / 100;
	min = in - (deg * 100);

	return deg + min / 60;
}

static int check_map_name(const char *fname, const char *mname)
{
	const char *p;

	p = fname + strlen(fname) - (3 + 1) - strlen(mname);
	if (strncasecmp(p, mname, strlen(mname)) == 0)
		return 1;
	return 0;
}

static int close_enough(double a, double b)
{
	return fabs(a - b) < 0.00001;
}

static void fix_map_coords(struct gps_map *map, const char *filename)
{
	int fixed = 0;

	if (check_map_name(filename, "M632")) {
		if (close_enough(map->area.start.la, 59 + 51.300 / 60)) {
			map->area.start.la += 0.100 / 60;
			fixed++;
		}
		if (close_enough(map->area.start.lo, 23 + 48.100 / 60)) {
			map->area.start.lo += 0.010 / 60;
			fixed++;
		}
	}

	if (fixed)
		printf("Fixed %d coordinate(s) of map %s\n", fixed, filename);
}

static int parse_in8(struct gpsnav *nav, struct gps_map *map,
		     const char *filename,
		     const struct gps_datum *from_dtm,
		     const struct gps_datum *to_dtm)
{
	FILE *in8_f;
	char buf[512];
	int found, ret;
	struct gps_key_value kv;

	in8_f = fopen(filename, "r");
	if (in8_f == NULL) {
		perror(filename);
		return -1;
	}
	found = 0;
	while (fgets(buf, sizeof(buf), in8_f) != NULL) {
		if (sscanf(buf, ";[NMEARECT] %lf,%lf,%lf,%lf", &map->area.start.lo,
			   &map->area.start.la, &map->area.end.lo, &map->area.end.la) == 4) {
			found = 1;
			break;
		}
	}
	if (!found)
		goto fail;



	map->area.start.la = parse_coord_value(map->area.start.la);
	map->area.start.lo = parse_coord_value(map->area.start.lo);
	map->area.end.la = parse_coord_value(map->area.end.la);
	map->area.end.lo = parse_coord_value(map->area.end.lo);
	map->datum = NULL; /* FIXME: Or should it be Hayford? */
	map->prov = gpsnav_find_provider(nav, mericd_provider.name);

	fix_map_coords(map, filename);

	/* the coordinates are in KKJ, so we convert them to WGS84 */
	gpsnav_convert_datum(&map->area.start, from_dtm, to_dtm);
	gpsnav_convert_datum(&map->area.end, from_dtm, to_dtm);

	if (find_gmb_file(in8_f, filename, buf, sizeof(buf)) < 0) {
		gps_error("%s: unable to locate .gmb file", buf);
		goto fail;
	}
	fclose(in8_f);

	kv.key = "gmb-filename";
	kv.value = strdup(buf);
	if (kv.value == NULL)
		return -1;

	ret = gpsnav_add_map(nav, map, &kv, 1, NULL);
	free(kv.value);
	if (ret < 0)
		return -1;

	return 0;
fail:
	fclose(in8_f);
	return -1;
}

static int mericd_scan_subdir(struct gpsnav *nav, struct gps_map_provider *prov,
			      const char *dirname, int nest)
{
	struct dirent *dent;
	DIR *dir;
	char buf[512];
	const struct gps_datum *from_datum, *to_datum;

	from_datum = gpsnav_find_datum(nav, "Finland Hayford");
	to_datum = gpsnav_find_datum(nav, "WGS 84");
	if (from_datum == NULL || to_datum == NULL) {
		gps_error("Unable to find correct datums from coordinate conversion");
		return -1;
	}

	dir = opendir(dirname);
	if (dir == NULL) {
		gps_error("%s: %s", dirname, strerror(errno));
		return -1;
	}

	while ((dent = readdir(dir)) != NULL) {
		struct gps_map *map;
		struct stat st;
		char *p;

		if (strcmp(dent->d_name, ".") == 0 ||
		    strcmp(dent->d_name, "..") == 0)
			continue;
		snprintf(buf, sizeof(buf), "%s/%s", dirname, dent->d_name);
		if (stat(buf, &st) < 0) {
			gps_error("%s: %s", buf, strerror(errno));
			continue;
		}
		if (S_ISDIR(st.st_mode)) {
			if (nest > 50) {
				gps_error("%s: nesting too far", buf);
				return -1;
			}
			mericd_scan_subdir(nav, prov, buf, nest + 1);
			continue;
		}
		if (!S_ISREG(st.st_mode))
			continue;
		p = strrchr(buf, '.');
		if (p == NULL)
			continue;
		if (strcasecmp(p, ".in8") != 0)
			continue;

		map = gps_map_new();
		if (map == NULL) {
			gps_error("malloc failed");
			return -ENOMEM;
		}
		if (parse_in8(nav, map, buf, from_datum, to_datum) != 0) {
			gps_error("%s: bad MeriCD map", buf);
			gps_map_free(map);
			continue;
		}
		/* We have a good map */
		printf("Map found: %s\n", buf);
	}
	closedir(dir);
	return 0;
}

int mericd_scan_directory(struct gpsnav *nav, const char *dirname)
{
	struct gps_map_provider *prov;

	prov = gpsnav_find_provider(nav, mericd_provider.name);
	if (prov == NULL) {
		gps_error("Unable to find MeriCD provider");
		return -1;
	}

	return mericd_scan_subdir(nav, prov, dirname, 0);
}

const char *mericd_get_gmb_filename(struct gps_map *map)
{
	struct gmb_map *gmap = map->data;

	return gmap->gmb_filename;
}
