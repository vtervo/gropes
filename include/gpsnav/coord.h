#ifndef GPSNAV_COORD_H
#define GPSNAV_COORD_H

struct gps_coord {
	double la;
	double lo;
};

struct gps_mcoord {
	double n;
	double e;
};

struct gps_area {
	struct gps_coord start; /* bottom-left */
	struct gps_coord end;   /* top-right */
};

struct gps_marea {
	struct gps_mcoord start;
	struct gps_mcoord end;
};

struct gps_ellipsoid {
	const char *name;
	double a;
	double invf;
};

struct gps_datum {
	const char *name;
	const struct gps_ellipsoid *ellipsoid;
	uint16_t dx, dy, dz;
};



extern void gpsnav_get_metric_for_coord(struct gps_map *map,
					const struct gps_coord *coord,
					struct gps_mcoord *out);
extern void gpsnav_get_coord_for_metric(struct gps_map *map,
					const struct gps_mcoord *mcoord,
					struct gps_coord *coord);

extern int gpsnav_get_xy_for_coord(struct gps_map *map,
				   const struct gps_coord *coord,
				   double *x_out, double *y_out);
extern int gpsnav_get_coord_for_xy(struct gps_map *map, int x, int y,
				   struct gps_coord *coord);
extern void gpsnav_calc_isect(const struct gps_area *a1,
			      const struct gps_area *a2,
			      struct gps_area *isect);
extern void gpsnav_calc_metric_isect(const struct gps_marea *a1,
				     const struct gps_marea *a2,
				     struct gps_marea *isect);
extern double gpsnav_calculate_area(struct gps_map *map,
				    const struct gps_area *area);

extern const struct gps_datum *gpsnav_find_datum(struct gpsnav *gpsnav, const char *name);
extern void gpsnav_convert_datum(struct gps_coord *coord,
				 const struct gps_datum *from,
				 const struct gps_datum *to);

extern const struct gps_datum gps_datum_table[];


static inline double deg2rad(double deg)
{
	return deg * M_PI / 180.0;
};

static inline double rad2deg(double rad)
{
	return rad / M_PI * 180.0;
};

#endif
