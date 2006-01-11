#include <string.h>
#include <errno.h>

#include <gpsnav/gpsnav.h>
#include <gpsnav/map.h>
#include <gpsnav/coord.h>
#include <lib_proj.h>

#include <jpeglib.h>
#include <png.h>
#include <gif_lib.h>

struct raster_map_type {
	char *tag;
	PJ *proj;
	char *proj_name;
	int lon0, false_easting;
	const struct gps_datum *datum;

	struct raster_map_type *next;
};

struct raster_map;

struct raster_decoder {
	const char *name;
	int (* read_header)(struct gps_map *map);
	int (* decode_pixels)(struct gps_map *map, unsigned char *out,
			      int x, int y, int width, int height,
			      int bpp, int row_stride);
};

struct raster_data {
	struct raster_map_type *type_list;
};

struct raster_map {
	char *bitmap_filename;
	const struct raster_decoder *dec;
	const struct raster_map_type *type;
};

static int read_jpeg_header(struct gps_map *map)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr err_mgr;
	struct raster_map *raster_map = map->data;
	int r;
	FILE *f;

	f = fopen(raster_map->bitmap_filename, "rb");
	if (f == NULL) {
		gps_error("%s: %s", raster_map->bitmap_filename, strerror(errno));
		return -1;
	}

	cinfo.err = jpeg_std_error(&err_mgr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, f);
	r = 0;
	if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
		gps_error("%s: invalid JPEG header", raster_map->bitmap_filename);
		r = -1;
	}
	if (r == 0) {
		map->width = cinfo.image_width;
		map->height = cinfo.image_height;
	}
	jpeg_destroy_decompress(&cinfo);
	fclose(f);
	return r;
}

static int decode_jpeg_pixels(struct gps_map *map,
			      unsigned char *out, int x, int y, int width,
			      int height, int bpp, int row_stride)
{
	struct raster_map *raster_map = map->data;
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr err_mgr;
	unsigned char *scan_line;
	int line;
	FILE *f;

	if (bpp != 24)
		return -1;
	f = fopen(raster_map->bitmap_filename, "rb");
	if (f == NULL) {
		gps_error("%s: %s", raster_map->bitmap_filename, strerror(errno));
		return -1;
	}

	cinfo.err = jpeg_std_error(&err_mgr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, f);
	if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
		gps_error("%s: invalid JPEG header", raster_map->bitmap_filename);
		goto fail;
	}
	jpeg_start_decompress(&cinfo);
	scan_line = malloc(cinfo.image_width * 3);
	for (line = 0; line < cinfo.image_height; line++) {
		jpeg_read_scanlines(&cinfo, &scan_line, 1);
		if (line < y || line >= y + height)
			continue;
		memcpy(out, scan_line + x * 3, width * 3);
		out += row_stride;
	}
	free(scan_line);
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	fclose(f);
	return 0;
fail:
	jpeg_destroy_decompress(&cinfo);
	fclose(f);
	return -1;
}

static const struct raster_decoder raster_jpeg_decoder = {
	.name = "jpeg",
	.read_header = read_jpeg_header,
	.decode_pixels = decode_jpeg_pixels,
};


static int read_png_header(struct gps_map *map)
{
	png_structp png;
	png_infop info;
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_type;
	struct raster_map *raster_map = map->data;
	int r;
	FILE *f;

	f = fopen(raster_map->bitmap_filename, "rb");
	if (f == NULL) {
		gps_error("%s: %s", raster_map->bitmap_filename, strerror(errno));
		return -1;
	}

	info = NULL;
	r = -1;

	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png == NULL) {
		gps_error("png_create_read_struct() failed");
		goto fail1;
	}

	info = png_create_info_struct(png);
	if (info == NULL)
		goto fail2;

	png_init_io(png, f);
	png_read_info(png, info);
	png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type,
		     &interlace_type, int_p_NULL, int_p_NULL);

	map->width = width;
	map->height = height;
	r = 0;
fail2:
	png_destroy_read_struct(&png, &info, png_infopp_NULL);
fail1:
	fclose(f);
	return r;
}

static int decode_png_pixels(struct gps_map *map,
			     unsigned char *out, int x, int y, int width,
			     int height, int bpp, int row_stride)
{
	struct raster_map *raster_map = map->data;
	unsigned char *scan_line;
	png_structp png;
	png_infop info;
	int line, r;
	FILE *f;

	if (bpp != 24)
		return -1;

	f = fopen(raster_map->bitmap_filename, "rb");
	if (f == NULL) {
		gps_error("%s: %s", raster_map->bitmap_filename, strerror(errno));
		return -1;
	}
	info = NULL;
	r = -1;

	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png == NULL) {
		gps_error("png_create_read_struct() failed");
		goto fail1;
	}

	info = png_create_info_struct(png);
	if (info == NULL)
		goto fail2;

	png_init_io(png, f);
	png_read_info(png, info);
	png_set_palette_to_rgb(png);
	if (width == map->width) {
		unsigned char **row_ptrs;
		int i;

		row_ptrs = malloc(sizeof(*row_ptrs) * (y + height));
		if (row_ptrs == NULL)
			goto fail2;
		for (i = 0; i < y + height; i++)
			if (i < y)
				row_ptrs[i] = NULL;
			else
				row_ptrs[i] = out + (i - y) * row_stride;
		png_read_rows(png, row_ptrs, NULL, y + height);
		free(row_ptrs);
	} else {
		scan_line = malloc(map->width * 3);
		for (line = 0; line < map->height; line++) {
			png_read_rows(png, &scan_line, png_bytepp_NULL, 1);
			if (line < y || line >= y + height)
				continue;
			memcpy(out, scan_line + x * 3, width * 3);
			out += row_stride;
		}
		free(scan_line);
	}
	r = 0;
fail2:
	png_destroy_read_struct(&png, &info, png_infopp_NULL);
fail1:
	fclose(f);
	return r;
}

static const struct raster_decoder raster_png_decoder = {
	.name = "png",
	.read_header = read_png_header,
	.decode_pixels = decode_png_pixels,
};


static int read_gif_header(struct gps_map *map)
{
	GifFileType *gif_file;
	struct raster_map *raster_map = map->data;

	gif_file = DGifOpenFileName(raster_map->bitmap_filename);
	if (gif_file == NULL) {
		gps_error("%s: %s", raster_map->bitmap_filename, strerror(errno));
		return -1;
	}
	map->width = gif_file->SWidth;
	map->height = gif_file->SHeight;
	DGifCloseFile(gif_file);
	return 0;
}

static int output_gif_pixels(struct gps_map *map, GifFileType *gf, int x, int y,
			     int width, int height, int row_stride, unsigned char *out)
{
	ColorMapObject *cm;
	unsigned char *scan_line;
	int r, col, line, cm_size;

	cm = gf->Image.ColorMap ? gf->Image.ColorMap : gf->SColorMap;
	cm_size = cm->ColorCount;

	r = -1;
	scan_line = malloc(map->width);
	for (line = 0; line < y + height; line++) {
		if (DGifGetLine(gf, scan_line, map->width) == GIF_ERROR) {
			PrintGifError();
			goto fail;
		}
		if (line < y)
			continue;
		for (col = 0; col < x + width; col++) {
			GifColorType *color;

			if (col < x)
				continue;
			if (scan_line[col] > cm_size)
				scan_line[col] = 0;
			color = &cm->Colors[scan_line[col]];
			*out++ = color->Red;
			*out++ = color->Green;
			*out++ = color->Blue;
		}
		out += row_stride - width * 3;
	}
	r = 0;
fail:
	free(scan_line);
	return r;
}

static int decode_gif_pixels(struct gps_map *map,
			     unsigned char *out, int x, int y, int width,
			     int height, int bpp, int row_stride)
{
	struct raster_map *raster_map = map->data;
	GifFileType *gf;
	GifRecordType record_type;
	int r;

	if (bpp != 24)
		return -1;
	gf = DGifOpenFileName(raster_map->bitmap_filename);
	if (gf == NULL) {
		gps_error("%s: %s", raster_map->bitmap_filename, strerror(errno));
		return -1;
	}
	r = -1;
	do {
		if (DGifGetRecordType(gf, &record_type) == GIF_ERROR) {
			PrintGifError();
			goto fail;
		}
		switch (record_type) {
		case IMAGE_DESC_RECORD_TYPE:
			if (DGifGetImageDesc(gf) == GIF_ERROR) {
				PrintGifError();
				goto fail;
			}
			if (gf->Image.Width != map->width ||
			    gf->Image.Height != map->height) {
				gps_error("%s: GIF subimages not supported",
					  raster_map->bitmap_filename);
				goto fail;
			}
			if (gf->Image.Interlace) {
				gps_error("%s: interlaced GIFs not supported",
					  raster_map->bitmap_filename);
				goto fail;
			}
			output_gif_pixels(map, gf, x, y, width, height, row_stride, out);
			break;
		case EXTENSION_RECORD_TYPE:
			gps_error("%s: GIF extensions not supported",
				  raster_map->bitmap_filename);
			goto fail;
		case UNDEFINED_RECORD_TYPE:
		case SCREEN_DESC_RECORD_TYPE:
		case TERMINATE_RECORD_TYPE:
			break;
		}
	} while (record_type != IMAGE_DESC_RECORD_TYPE);
	r = 0;
fail:
	DGifCloseFile(gf);
	return r;
}

static const struct raster_decoder raster_gif_decoder = {
	.name = "gif",
	.read_header = read_gif_header,
	.decode_pixels = decode_gif_pixels,
};


static void raster_add_type(struct raster_data *data, struct raster_map_type *type)
{
	type->next = data->type_list;
	data->type_list = type;
}

static struct raster_map_type *raster_find_type(struct raster_data *data,
						const char *tag)
{
	struct raster_map_type *type;

	for (type = data->type_list; type != NULL; type = type->next) {
		if (type->tag == NULL)
			continue;
		if (strcmp(type->tag, tag) == 0)
			return type;
	}
	return NULL;
}

static void raster_free_map(struct gps_map *map)
{
	struct raster_map *raster_map = map->data;

	free(raster_map->bitmap_filename);
	free(raster_map);
}

static int raster_check_for_dupe(struct gpsnav *nav, const char *fname)
{
	struct gps_map *e;

	for (e = nav->map_list.lh_first; e != NULL; e = e->entries.le_next) {
		struct raster_map *rm;

		if (strcmp(e->prov->name, "raster") != 0)
			continue;
		rm = e->data;
		if (strcmp(rm->bitmap_filename, fname) == 0)
			return -1;
	}
	return 0;
}

static int raster_add_map(struct gpsnav *nav, struct gps_map *map,
			  struct gps_key_value *kv, int kv_count,
			  const char *base_path)
{
	struct raster_map *raster_map;
	struct raster_map_type *type;
	struct raster_data *data = map->prov->data;
	const char *bitmap_type, *tag;
	const char *datum_str, *proj_str;
	char *bitmap_filename;
	const struct gps_datum *datum;
	PJ *pj;
	int lon0, false_easting;
	int i, r;

	bitmap_filename = NULL;
	bitmap_type = tag = datum_str = proj_str = NULL;
	lon0 = false_easting = 0;
	for (i = 0; i < kv_count; i++, kv++) {
		if (strcmp(kv->key, "bitmap-filename") == 0)
			bitmap_filename = kv->value;
		else if (strcmp(kv->key, "bitmap-type") == 0)
			bitmap_type = kv->value;
		else if (strcmp(kv->key, "tag") == 0)
			tag = kv->value;
		else if (strcmp(kv->key, "datum") == 0)
			datum_str = kv->value;
		else if (strcmp(kv->key, "projection") == 0)
			proj_str = kv->value;
		else if (strcmp(kv->key, "central-meridian") == 0) {
			if (sscanf(kv->value, "%d", &lon0) != 1 ||
			    lon0 < -180 || lon0 > 180) {
				gps_error("Invalid central meridian value");
				return -1;
			}
		} else if (strcmp(kv->key, "false-easting") == 0) {
			if (sscanf(kv->value, "%d", &false_easting) != 1) {
				gps_error("Invalid false easting value");
				return -1;
			}
		}
	}

	if (bitmap_filename == NULL) {
		gps_error("Did not get bitmap filename");
		return -1;
	}

	bitmap_filename = gpsnav_get_full_path(bitmap_filename, base_path);
	if (bitmap_filename == NULL)
		return -1;

	r = -1;

	if (raster_check_for_dupe(nav, bitmap_filename) < 0) {
		r = -EEXIST;
		goto fail;
	}
	if (bitmap_type == NULL) {
		gps_error("Did not get bitmap type");
		goto fail;
	}

	if (tag == NULL || (type = raster_find_type(data, tag)) == NULL) {
		const struct gps_ellipsoid *ell;
		char buf[5][40], *projc[5];
		int c, i;

		if (datum_str == NULL) {
			gps_error("Datum not specified");
			goto fail;
		}
		datum = gpsnav_find_datum(nav, datum_str);
		if (datum == NULL) {
			gps_error("Unknown datum '%s'", datum_str);
			goto fail;
		}
		if (proj_str == NULL) {
			gps_error("Map projection not specified");
			goto fail;
		}
		ell = datum->ellipsoid;
		c = 0;
		sprintf(buf[c++], "proj=%s", proj_str);
		sprintf(buf[c++], "a=%f", ell->a);
		sprintf(buf[c++], "rf=%f", ell->invf);
		if (lon0)
			sprintf(buf[c++], "lon_0=%d", lon0);
		if (false_easting)
			sprintf(buf[c++], "x_0=%d", false_easting);
		for (i = 0; i < c; i++)
			projc[i] = buf[i];
		pj = pj_init(c, projc);
		if (pj == NULL) {
			gps_error("Invalid projection variables");
			goto fail;
		}
		type = malloc(sizeof(*type));
		if (type == NULL) {
			pj_free(pj);
			r = -ENOMEM;
			goto fail;
		}
		if (tag != NULL) {
			type->tag = strdup(tag);
			if (type->tag == NULL) {
				pj_free(pj);
				free(type);
				r = -ENOMEM;
				goto fail;
			}
		} else
			type->tag = NULL;
		type->proj_name = strdup(proj_str);
		if (type->proj_name == NULL) {
			pj_free(pj);
			free(type->tag);
			free(type);
			r = -ENOMEM;
			goto fail;
		}
		type->datum = datum;
		type->proj = pj;
		type->lon0 = lon0;
		type->false_easting = false_easting;
		raster_add_type(data, type);
	}

	raster_map = malloc(sizeof(*raster_map));
	if (raster_map == NULL) {
		r = -ENOMEM;
		goto fail;
	}
	if (strcmp(bitmap_type, raster_png_decoder.name) == 0)
		raster_map->dec = &raster_png_decoder;
	else if (strcmp(bitmap_type, raster_jpeg_decoder.name) == 0)
		raster_map->dec = &raster_jpeg_decoder;
	else if (strcmp(bitmap_type, raster_gif_decoder.name) == 0)
		raster_map->dec = &raster_gif_decoder;
	else {
		gps_error("Invalid raster bitmap type '%s'", bitmap_type);
		free(raster_map);
		goto fail;
	}

	raster_map->type = type;
	raster_map->bitmap_filename = bitmap_filename;

	map->datum = type->datum;
	map->proj = type->proj;
	map->data = raster_map;

	if (raster_map->dec->read_header(map) < 0) {
		raster_free_map(map);
		map->data = NULL;
		goto fail;
	}
	return 0;
fail:
	free(bitmap_filename);
	return r;
}

static int raster_get_map_info(struct gpsnav *nav, struct gps_map *map,
			       struct gps_key_value **kv_out, int *kv_count,
			       const char *base_path)
{
	struct raster_map *raster_map = map->data;
	struct gps_key_value *kv;
	int c;

	c = 3;
	if (raster_map->type->tag != NULL)
		c++;
	if (map->datum != NULL)
		c++;
	if (raster_map->type->lon0 != 0)
		c++;
	if (raster_map->type->false_easting != 0)
		c++;
	kv = malloc(sizeof(*kv) * c);
	if (kv == NULL)
		return -ENOMEM;
	c = 0;
	/* FIXME: check strdup() return values */
	kv[c].key = strdup("bitmap-filename");
	kv[c++].value = gpsnav_remove_base_path(raster_map->bitmap_filename,
						base_path);
	kv[c].key = strdup("bitmap-type");
	kv[c++].value = strdup(raster_map->dec->name);
	if (raster_map->type->tag != NULL) {
		kv[c].key = strdup("tag");
		kv[c++].value = strdup(raster_map->type->tag);
	}
	if (map->datum != NULL) {
		kv[c].key = strdup("datum");
		kv[c++].value = strdup(map->datum->name);
	}
	kv[c].key = strdup("projection");
	kv[c++].value = strdup(raster_map->type->proj_name);
	if (raster_map->type->lon0 != 0) {
		char buf[16];

		kv[c].key = strdup("central-meridian");
		sprintf(buf, "%d", raster_map->type->lon0);
		kv[c++].value = strdup(buf);
	}
	if (raster_map->type->false_easting != 0) {
		char buf[16];

		kv[c].key = strdup("false-easting");
		sprintf(buf, "%d", raster_map->type->false_easting);
		kv[c++].value = strdup(buf);
	}

	*kv_out = kv;
	*kv_count = c;

	return 0;
}

static int raster_get_pixels(struct gpsnav *nav, struct gps_map *map,
			     struct gps_pixel_buf *pb)
{
	struct raster_map *raster_map = map->data;
	int r;

	r = raster_map->dec->decode_pixels(map, pb->data, pb->x, pb->y,
					   pb->width, pb->height,
					   pb->bpp, pb->row_stride);
	if (r < 0)
		return -1;
	return 0;
}

static int raster_init(struct gpsnav *gpsnav, struct gps_map_provider *prov)
{
	struct raster_data *data;

	data = malloc(sizeof(*data));
	if (data == NULL)
		return -ENOMEM;
	memset(data, 0, sizeof(*data));
	prov->data = data;

	return 0;
}

static void raster_finish(struct gpsnav *nav, struct gps_map_provider *prov)
{
	struct raster_data *data = prov->data;
	struct raster_map_type *type;

	type = data->type_list;
	while (type != NULL) {
		struct raster_map_type *next;

		pj_free(type->proj);
		if (type->tag != NULL)
			free(type->tag);
		free(type->proj_name);
		next = type->next;
		free(type);
		type = next;
	}
	free(data);
}

struct gps_map_provider raster_provider = {
	.name = "raster",
	.get_pixels = raster_get_pixels,
	.add_map = raster_add_map,
	.get_map_info = raster_get_map_info,
	.free_map = raster_free_map,
	.init = raster_init,
	.finish = raster_finish
};
