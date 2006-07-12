#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <lib_proj.h>

#include <gpsnav/gpsnav.h>
#include <gpsnav/map.h>
#include <gpsnav/coord.h>
#include <gpsnav/pixcache.h>

static int calculate_map_scale(struct gps_map *map)
{
	double scale_rat;

	gpsnav_get_metric_for_coord(map, &map->area.start, &map->marea.start);
	gpsnav_get_metric_for_coord(map, &map->area.end, &map->marea.end);
	map->scale_y = (map->marea.end.n - map->marea.start.n) / map->height;
	map->scale_x = (map->marea.end.e - map->marea.start.e) / map->width;
	scale_rat = fabs(1.0 - map->scale_y / map->scale_x);
#if 0
	printf("Y scale %f, X scale %f, %f\n", map->scale_y, map->scale_x,
	       scale_rat);
	/* Sanity check */
	if (scale_rat > 0.001)
		return -1;
#endif

	return 0;
}

int gpsnav_add_map(struct gpsnav *gpsnav, struct gps_map *map,
		   struct gps_key_value *kv, int kv_count,
		   const char *base_path)
{
	int r;

	if (map->prov->add_map != NULL) {
		r = map->prov->add_map(gpsnav, map, kv, kv_count,
				       base_path);
		if (r < 0)
			return r;
	}

	r = calculate_map_scale(map);
	if (r < 0) {
		gps_error("Invalid map scale");
		map->prov->free_map(map);
		map->data = NULL;
		return -1;
	}

	LIST_INSERT_HEAD(&gpsnav->map_list, map, entries);

	return 0;
}

struct gps_map *gps_map_new(void)
{
	struct gps_map *map;

	map = malloc(sizeof(*map));
	if (map == NULL)
		return NULL;
	memset(map, 0, sizeof(*map));

	return map;
}

void gps_map_free(struct gps_map *map)
{
	if (map->prov != NULL && map->prov->free_map != NULL &&
	    map->data != NULL)
		map->prov->free_map(map);
	free(map);
}

int gpsnav_get_map_pixels(struct gpsnav *nav, struct gps_map *map,
			  struct gps_pixel_buf *pb)
{
	int r, can_add;

	if (gpsnav_pixcache_get(nav, map, pb) == 0)
		return 0;
	if (pb->data == NULL) {
		int bytes_pp;

		if (pb->bpp > 0)
			bytes_pp = pb->bpp / 8;
		else {
			bytes_pp = 3;
			pb->bpp = 24;
		}

		/* If the map is small enough, it probably is more efficient
		 * to decode the whole map */
		if (map->width * map->height * bytes_pp < nav->pc_max_size) {
			pb->x = pb->y = 0;
			pb->width = map->width;
			pb->height = map->height;
			pb->bpp = bytes_pp * 8;
		}
		pb->row_stride = pb->width * bytes_pp;
		pb->data = malloc(pb->width * pb->height * bytes_pp);
		if (pb->data == NULL) {
			gps_error("malloc failed");
			return -ENOMEM;
		}
		can_add = 1;
	} else
		can_add = 0;

	r = map->prov->get_pixels(nav, map, pb);
	if (r < 0) {
		if (can_add)
			free(pb->data);
		return r;
	}

	if (can_add) {
		r = gpsnav_pixcache_add(nav, map, pb);
		if (r == 0)
			pb->can_free = 0;
		else
			pb->can_free = 1;
	}

	return 0;
}

static int within_rectangle(struct gps_coord *coord, struct gps_coord *start,
			    struct gps_coord *end)
{
	if (coord->la < start->la || coord->lo < start->lo)
		return 0;
	if (coord->la > end->la || coord->lo > end->lo)
		return 0;
	return 1;
}

static int coord_in_map(struct gps_map *map, struct gps_coord *coord)
{
	return within_rectangle(coord, &map->area.start, &map->area.end);
}

struct gps_map **gpsnav_find_maps(struct gpsnav *gpsnav,
					 int (* check_map)(struct gps_map *map, void *arg),
					 void *arg)
{
	struct gps_map **map_list;
	struct gps_map *map;
	int c;

	map_list = NULL;
	c = 0;
	for (map = gpsnav->map_list.lh_first; map != NULL; map = map->entries.le_next) {
		if (check_map != NULL)
			if (!check_map(map, arg))
				continue;

		map_list = realloc(map_list, sizeof(*map_list) * (c + 2));
		if (map_list == NULL)
			return NULL;
		map_list[c++] = map;
	}
	if (map_list != NULL)
		map_list[c] = NULL;
	return map_list;
}

static int check_map_for_coord(struct gps_map *map, void *arg)
{
	return coord_in_map(map, (struct gps_coord *) arg);
}

struct gps_map **gpsnav_find_maps_for_coord(struct gpsnav *gpsnav,
					    struct gps_coord *coord)
{
	return gpsnav_find_maps(gpsnav, check_map_for_coord, coord);
}

void gpsnav_calc_isect(const struct gps_area *a1,
		       const struct gps_area *a2,
		       struct gps_area *res)
{
	const struct gps_coord *s1, *s2, *e1, *e2;
	struct gps_coord *s_res, *e_res;

	s1 = &a1->start;
	s2 = &a2->start;
	e1 = &a1->end;
	e2 = &a2->end;
	s_res = &res->start;
	e_res = &res->end;

	/* Choose bigger values for the intersection start coord */
	if (s1->la > s2->la)
		s_res->la = s1->la;
	else
		s_res->la = s2->la;
	if (s1->lo > s2->lo)
		s_res->lo = s1->lo;
	else
		s_res->lo = s2->lo;

	/* Choose smaller values for the intersection end coord */
	if (e1->la < e2->la)
		e_res->la = e1->la;
	else
		e_res->la = e2->la;
	if (e1->lo < e2->lo)
		e_res->lo = e1->lo;
	else
		e_res->lo = e2->lo;
}

void gpsnav_calc_metric_isect(const struct gps_marea *a1,
			      const struct gps_marea *a2,
			      struct gps_marea *res)
{
	return gpsnav_calc_isect((const struct gps_area *) a1,
				 (const struct gps_area *) a2,
				 (struct gps_area *) res);
}

static int check_map_for_area(struct gps_map *map, void *arg)
{
	const struct gps_area *area = arg;
	struct gps_area isect;

	gpsnav_calc_isect(&map->area, area, &isect);
	if (isect.start.lo >= isect.end.lo ||
	    isect.start.la >= isect.end.la)
		return 0;
	return 1;
}

struct gps_map **gpsnav_find_maps_for_area(struct gpsnav *gpsnav,
					   const struct gps_area *area)
{
	return gpsnav_find_maps(gpsnav, check_map_for_area, (void *) area);
}

struct marea_arg {
	PJ *pj;
	const struct gps_marea *area;
};

static int check_map_for_marea(struct gps_map *map, void *arg)
{
	struct marea_arg *marg = arg;
	struct gps_marea isect;
	const struct gps_marea *area = marg->area;
	PJ *pj = marg->pj;
	PJ *mpj = map->proj;

	/* If the projections are not alike, we won't display
	 * the maps at the same time */
	if (pj->descr != mpj->descr ||
	    pj->lam0 != mpj->lam0)
		return 0;

	gpsnav_calc_metric_isect(&map->marea, area, &isect);

	if (isect.start.n >= isect.end.n ||
	    isect.start.e >= isect.end.e)
		return 0;
	return 1;
}

struct gps_map **gpsnav_find_maps_for_marea(struct gpsnav *gpsnav,
					    struct gps_map *ref_map,
					    const struct gps_marea *marea)
{
	struct marea_arg arg;
	PJ *proj = ref_map->proj;

	arg.pj = proj;
	arg.area = marea;
	return gpsnav_find_maps(gpsnav, check_map_for_marea, (void *) &arg);
}

void gpsnav_get_metric_for_coord(struct gps_map *map,
				 const struct gps_coord *coord,
				 struct gps_mcoord *out)
{
	struct gps_coord map_coord;
	LP lp;
	XY xy;

	if (map->datum != NULL) {
		map_coord = *coord;
		coord = &map_coord;
		gpsnav_convert_datum(&map_coord, NULL, map->datum);
	}
	lp.lam = deg2rad(coord->lo);
	lp.phi = deg2rad(coord->la);
//	printf("lam %f, phi %f\n", lp.lam, lp.phi);
	xy = pj_fwd(lp, map->proj);
//	printf("x %f, y %f\n", xy.x, xy.y);
	out->n = xy.y;
	out->e = xy.x;
}

void gpsnav_get_coord_for_metric(struct gps_map *map,
				 const struct gps_mcoord *mcoord,
				 struct gps_coord *coord)
{
	LP lp;
	XY xy;

	xy.y = mcoord->n;
	xy.x = mcoord->e;
	lp = pj_inv(xy, map->proj);
	coord->lo = rad2deg(lp.lam);
	coord->la = rad2deg(lp.phi);
	if (map->datum != NULL)
		gpsnav_convert_datum(coord, map->datum, NULL);
}

int gpsnav_get_xy_for_coord(struct gps_map *map, const struct gps_coord *coord,
			    double *x_out, double *y_out)
{
	struct gps_mcoord mcoord;

	gpsnav_get_metric_for_coord(map, coord, &mcoord);

	*y_out = map->height - (mcoord.n - map->marea.start.n) / map->scale_y;
	*x_out = (mcoord.e - map->marea.start.e) / map->scale_x;

	return 0;
}

int gpsnav_get_coord_for_xy(struct gps_map *map, int x, int y,
			    struct gps_coord *coord_out)
{
	struct gps_mcoord mc;

	mc.n = map->marea.start.n + (map->height - y) * map->scale_y;
	mc.e = map->marea.start.e + x * map->scale_x;
	gpsnav_get_coord_for_metric(map, &mc, coord_out);

	return 0;
}

double gpsnav_calculate_area(struct gps_map *map, const struct gps_area *area)
{
	struct gps_mcoord mc1, mc2;

	gpsnav_get_metric_for_coord(map, &area->start, &mc1);
	gpsnav_get_metric_for_coord(map, &area->end, &mc2);

	return fabs((mc1.n - mc2.n) * (mc1.e - mc2.e));
}

int gpsnav_get_provider_map_info(struct gpsnav *nav, struct gps_map *map,
				 struct gps_key_value **kv_out,
				 int *kv_count, const char *base_path)
{
	return map->prov->get_map_info(nav, map, kv_out, kv_count, base_path);
}
