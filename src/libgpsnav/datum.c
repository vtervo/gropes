#include <stdint.h>
#include <math.h>
#include <string.h>

#include <gpsnav/gpsnav.h>
#include <gpsnav/coord.h>

#define GPS_DATUM_WGS84			0
#define GPS_DATUM_FINLAND_HAYFORD	1

static const struct gps_ellipsoid clarke_1866 =
{ "Clarke 1866",		6378206.4,   294.9786982   };

static const struct gps_ellipsoid grs_80 =
{ "GRS 80",			6378137.0,   298.257222101 };

static const struct gps_ellipsoid wgs_84 =
{ "WGS 84",			6378137.0,   298.257223563 };

static const struct gps_ellipsoid intl_1924 =
{ "International 1924",		6378388.0,   297.0	   };

const struct gps_datum gps_datum_table[] = {
	{ "WGS 84",		&wgs_84,	    0,	    0,	    0 },
	{ "Finland Hayford",	&intl_1924,	  -78,	 -231,	  -97 },
	{ "European 1950",	&intl_1924,	  -87,	  -98,	 -121 },
	{ "European 1979",	&intl_1924,	  -86,	  -98,	 -119 },
	{ "NAD27 Alaska",	&clarke_1866,	   -5,	  135,	  172 },
	{ "NAD27 Bahamas",	&clarke_1866,	   -4,	  154,	  178 },
	{ "NAD27 Canada",	&clarke_1866,	  -10,	  158,	  187 },
	{ "NAD27 Canal Zone",	&clarke_1866,	    0,	  125,	  201 },
	{ "NAD27 Caribbean",	&clarke_1866,	   -7,	  152,	  178 },
	{ "NAD27 Central",	&clarke_1866,	    0,	  125,	  194 },
	{ "NAD27 CONUS",	&clarke_1866,	   -8,	  160,	  176 },
	{ "NAD27 Cuba", 	&clarke_1866,	   -9,	  152,	  178 },
	{ "NAD27 Greenland",	&clarke_1866,	   11,	  114,	  195 },
	{ "NAD27 Mexico",	&clarke_1866,	  -12,	  130,	  190 },
	{ "NAD27 San Salvador", &clarke_1866,	    1,	  140,	  165 },
	{ "NAD83",		&grs_80,	    0,	    0,	    0 },
};	

#if 0
const struct gps_ellipsoid gps_ellipsoid_table[] = {
	/*  name			       a	1/f	      */
	{ "Airy 1830",			6377563.396, 299.3249646   }, /* 0 */
	{ "Modified Airy",		6377340.189, 299.3249646   },
	{ "Australian National",	6378160.0,   298.25	   },
	{ "Bessel 1841",		6377397.155, 299.15281282  },
	{ "Clarke 1866",		6378206.4,   294.9786982   },
	{ "Clarke 1880",		6378249.145, 293.465	   },
	{ "Everest (India 1830)",	6377276.345, 300.8017	   },
	{ "Everest (1948)",		6377304.063, 300.8017	   },
	{ "Modified Fischer 1960",	6378155.0,   298.3	   },
	{ "Everest (Pakistan)",		6377309.613, 300.8017	   },
	{ "Indonesian 1974",		6378160.0,   298.247	   }, /* 10 */
	{ "GRS 80",			6378137.0,   298.257222101 },
	{ "Helmert 1906",		6378200.0,   298.3	   },
	{ "Hough 1960",			6378270.0,   297.0	   },
	{ "International 1924",		6378388.0,   297.0	   },
	{ "Krassovsky 1940",		6378245.0,   298.3	   },
	{ "South American 1969",	6378160.0,   298.25	   },
	{ "Everest (Malaysia 1969)",	6377295.664, 300.8017	   },
	{ "Everest (Sabah Sarawak)",	6377298.556, 300.8017	   },
	{ "WGS 72",			6378135.0,   298.26	   },
	{ "WGS 84",			6378137.0,   298.257223563 }, /* 20 */
	{ "Bessel 1841 (Namibia)",	6377483.865, 299.1528128   },
	{ "Everest (India 1956)",	6377301.243, 300.8017	   },
	{ "Fischer 1960",		6378166.0,   298.3	   },
	{ "WGS 60",			6378165.0,   298.3	   },
	{ "WGS 66",			6378145.0,   298.25	   },
	{ "SGS 85",			6378136.0,   298.257	   }, /* 26 */
	{ NULL },
};

const struct gps_datum gps_datum_table[] = {
	/* name 	      ellipsoid   dx	  dy	   dz	      */
	{ "WGS 84",		   20,	    0,	    0,	    0 },
	{ "Adindan",		    5,	 -162,	  -12,	  206 },
	{ "Afgooye",		   15,	  -43,	 -163,	   45 },
	{ "Ain el Abd 1970",	   14,	 -150,	 -251,	   -2 },
	{ "Anna 1 Astro 1965",	    2,	 -491,	  -22,	  435 },
	{ "Arc 1950",		    5,	 -143,	  -90,	 -294 },
	{ "Arc 1960",		    5,	 -160,	   -8,	 -300 },
	{ "Ascension Island '58",  14,	 -207,	  107,	   52 },
	{ "Astro B4 Sorol Atoll",  14,	  114,	 -116,	 -333 },
	{ "Astro Beacon \"E\"",	   14,	  145,	   75,	 -272 },
	{ "Astro DOS 71/4",	   14,	 -320,	  550,	 -494 },
	{ "Astronomic Stn '52",    14,	  124,	 -234,	  -25 },
	{ "Australian Geod '66",    2,	 -133,	  -48,	  148 },
	{ "Australian Geod '84",    2,	 -134,	  -48,	  149 },
	{ "Bellevue (IGN)",	   14,	 -127,	 -769,	  472 },
	{ "Bermuda 1957",	    4,	  -73,	  213,	  296 },
	{ "Bogota Observatory",    14,	  307,	  304,	 -318 },
	{ "Campo Inchauspe",	   14,	 -148,	  136,	   90 },
	{ "Canton Astro 1966",	   14,	  298,	 -304,	 -375 },
	{ "Cape",		    5,	 -136,	 -108,	 -292 },
	{ "Cape Canaveral",	    4,	   -2,	  150,	  181 },
	{ "Carthage",		    5,	 -263,	    6,	  431 },
	{ "CH-1903",		    3,	  674,	   15,	  405 },
	{ "Chatham 1971",	   14,	  175,	  -38,	  113 },
	{ "Chua Astro", 	   14,	 -134,	  229,	  -29 },
	{ "Corrego Alegre",	   14,	 -206,	  172,	   -6 },
	{ "Djakarta (Batavia)",     3,	 -377,	  681,	  -50 },
	{ "DOS 1968",		   14,	  230,	 -199,	 -752 },
	{ "Easter Island 1967",    14,	  211,	  147,	  111 },
	{ "European 1950",	   14,	  -87,	  -98,	 -121 },
	{ "European 1979",	   14,	  -86,	  -98,	 -119 },
	{ "Finland Hayford",	   14,	  -78,	 -231,	  -97 },
	{ "Gandajika Base",	   14,	 -133,	 -321,	   50 },
	{ "Geodetic Datum '49",    14,	   84,	  -22,	  209 },
	{ "Guam 1963",		    4,	 -100,	 -248,	  259 },
	{ "GUX 1 Astro",	   14,	  252,	 -209,	 -751 },
	{ "Hjorsey 1955",	   14,	  -73,	   46,	  -86 },
	{ "Hong Kong 1963",	   14,	 -156,	 -271,	 -189 },
	{ "Indian Bangladesh",	    6,	  289,	  734,	  257 },
	{ "Indian Thailand",	    6,	  214,	  836,	  303 },
	{ "Ireland 1965",	    1,	  506,	 -122,	  611 },
	{ "ISTS 073 Astro '69",    14,	  208,	 -435,	 -229 },
	{ "Johnston Island",	   14,	  191,	  -77,	 -204 },
	{ "Kandawala",		    6,	  -97,	  787,	   86 },
	{ "Kerguelen Island",	   14,	  145,	 -187,	  103 },
	{ "Kertau 1948",	    7,	  -11,	  851,	    5 },
	{ "L.C. 5 Astro",	    4,	   42,	  124,	  147 },
	{ "Liberia 1964",	    5,	  -90,	   40,	   88 },
	{ "Luzon Mindanao",	    4,	 -133,	  -79,	  -72 },
	{ "Luzon Philippines",	    4,	 -133,	  -77,	  -51 },
	{ "Mahe 1971",		    5,	   41,	 -220,	 -134 },
	{ "Marco Astro",	   14,	 -289,	 -124,	   60 },
	{ "Massawa",		    3,	  639,	  405,	   60 },
	{ "Merchich",		    5,	   31,	  146,	   47 },
	{ "Midway Astro 1961",	   14,	  912,	  -58,	 1227 },
	{ "Minna",		    5,	  -92,	  -93,	  122 },
	{ "NAD27 Alaska",	    4,	   -5,	  135,	  172 },
	{ "NAD27 Bahamas",	    4,	   -4,	  154,	  178 },
	{ "NAD27 Canada",	    4,	  -10,	  158,	  187 },
	{ "NAD27 Canal Zone",	    4,	    0,	  125,	  201 },
	{ "NAD27 Caribbean",	    4,	   -7,	  152,	  178 },
	{ "NAD27 Central",	    4,	    0,	  125,	  194 },
	{ "NAD27 CONUS",	    4,	   -8,	  160,	  176 },
	{ "NAD27 Cuba", 	    4,	   -9,	  152,	  178 },
	{ "NAD27 Greenland",	    4,	   11,	  114,	  195 },
	{ "NAD27 Mexico",	    4,	  -12,	  130,	  190 },
	{ "NAD27 San Salvador",     4,	    1,	  140,	  165 },
	{ "NAD83",		   11,	    0,	    0,	    0 },
	{ "Nahrwn Masirah Ilnd",    5,	 -247,	 -148,	  369 },
	{ "Nahrwn Saudi Arbia",     5,	 -231,	 -196,	  482 },
	{ "Nahrwn United Arab",     5,	 -249,	 -156,	  381 },
	{ "Naparima BWI",	   14,	   -2,	  374,	  172 },
	{ "Observatorio 1966",	   14,	 -425,	 -169,	   81 },
	{ "Old Egyptian",	   12,	 -130,	  110,	  -13 },
	{ "Old Hawaiian",	    4,	   61,	 -285,	 -181 },
	{ "Oman",		    5,	 -346,	   -1,	  224 },
	{ "Ord Srvy Grt Britn",     0,	  375,	 -111,	  431 },
	{ "Pico De Las Nieves",    14,	 -307,	  -92,	  127 },
	{ "Pitcairn Astro 1967",   14,	  185,	  165,	   42 },
	{ "Prov So Amrican '56",   14,	 -288,	  175,	 -376 },
	{ "Prov So Chilean '63",   14,	   16,	  196,	   93 },
	{ "Puerto Rico",	    4,	   11,	   72,	 -101 },
	{ "Qatar National",	   14,	 -128,	 -283,	   22 },
	{ "Qornoq",		   14,	  164,	  138,	 -189 },
	{ "Reunion",		   14,	   94,	 -948,	-1262 },
	{ "Rome 1940",		   14,	 -225,	  -65,	    9 },
	{ "RT 90",		    3,	  498,	  -36,	  568 },
	{ "Santo (DOS)",	   14,	  170,	   42,	   84 },
	{ "Sao Braz",		   14,	 -203,	  141,	   53 },
	{ "Sapper Hill 1943",	   14,	 -355,	   16,	   74 },
	{ "Schwarzeck", 	   21,	  616,	   97,	 -251 },
	{ "South American '69",    16,	  -57,	    1,	  -41 },
	{ "South Asia", 	    8,	    7,	  -10,	  -26 },
	{ "Southeast Base",	   14,	 -499,	 -249,	  314 },
	{ "Southwest Base",	   14,	 -104,	  167,	  -38 },
	{ "Timbalai 1948",	    6,	 -689,	  691,	  -46 },
	{ "Tokyo",		    3,	 -128,	  481,	  664 },
	{ "Tristan Astro 1968",    14,	 -632,	  438,	 -609 },
	{ "Viti Levu 1916",	    5,	   51,	  391,	  -36 },
	{ "Wake-Eniwetok '60",	   13,	  101,	   52,	  -39 },
	{ "WGS 72",		   19,	    0,	    0,	    5 },
	{ "Zanderij",		   14,	 -265,	  120,	 -358 },
	{ NULL },
};
#endif

static const double wgs84_a	= 6378137.0;		/* WGS84 semimajor axis */
static const double wgs84_invf	= 298.257223563;	/* WGS84 1/f */

static void molod_translate(struct gps_coord *coord,
			    const struct gps_datum *other_d, int from_wgs84)
{
	double phi;
	double lambda;
	double a0, b0, es0, f0;               /* Reference ellipsoid of input data */
	double a1, b1, es1, f1;              /* Reference ellipsoid of output data */
	double psi;                                         /* geocentric latitude */
	double x, y, z;           /* 3D coordinates with respect to original datum */
	double psi1;                            /* transformed geocentric latitude */
	double dx, dy, dz, a, f;

	dx = other_d->dx;
	dy = other_d->dy;
	dz = other_d->dz;
	a = other_d->ellipsoid->a;
	f = 1.0 / other_d->ellipsoid->invf;

	phi = coord->la * M_PI / 180.0;
	lambda = coord->lo * M_PI / 180.0;

	if (from_wgs84) {	       /* convert from WGS84 to new datum */
		a0 = wgs84_a;	       /* WGS84 semimajor axis */
		f0 = 1.0 / wgs84_invf; /* WGS84 flattening */
		a1 = a;
		f1 = f;
	} else {                       /* convert from datum to WGS84 */
		a0 = a;                /* semimajor axis */
		f0 = f;                /* flattening */
		a1 = wgs84_a;          /* WGS84 semimajor axis */
		f1 = 1.0 / wgs84_invf; /* WGS84 flattening */
		dx = -dx;
		dy = -dy;
		dz = -dz;
	}

	b0 = a0 * (1 - f0);                     /* semiminor axis for input datum */
	es0 = 2 * f0 - f0*f0;                   /* eccentricity^2 */

	b1 = a1 * (1 - f1);                     /* semiminor axis for output datum */
	es1 = 2 * f1 - f1*f1;                   /* eccentricity^2 */

	/* Convert geodedic latitude to geocentric latitude, psi */
	if (coord->la == 0.0 || coord->la == 90.0 || coord->la == -90.0)
		psi = phi;
	else
		psi = atan((1 - es0) * tan(phi));

	/* Calc x and y axis coordinates with respect to original ellipsoid */
	if (coord->lo == 90.0 || coord->lo == -90.0) {
		x = 0.0;
		y = fabs(a0 * b0 / sqrt(b0*b0 + a0*a0 * pow(tan(psi), 2.0)));
	} else {
		x = fabs((a0 * b0) /
			 sqrt((1 + pow(tan(lambda), 2.0)) *
			      (b0*b0 + a0*a0 * pow(tan(psi), 2.0))));
		y = fabs(x * tan(lambda));
	}

	if (coord->lo < -90.0 || coord->lo > 90.0)
		x = -x;
	if (coord->lo < 0.0)
		y = -y;

	/* Calculate z axis coordinate with respect to the original ellipsoid */
	if (coord->la == 90.0)
		z = b0;
	else if (coord->la == -90.0)
		z = -b0;
	else
		z = tan(psi) * sqrt((a0*a0 * b0*b0) / (b0*b0 + a0*a0 * pow(tan(psi), 2.0)));

	/* Calculate the geodetic latitude with respect to the new ellipsoid */
	psi1 = atan((z - dz) / sqrt((x - dx)*(x - dx) + (y - dy)*(y - dy)));

	/* Convert to geocentric latitude and save return value */
	coord->la = atan(tan(psi1) / (1 - es1)) * 180.0 / M_PI;

	/* Calculate the longitude with respect to the new ellipsoid */
	coord->lo = atan((y - dy) / (x - dx)) * 180.0 / M_PI;

	/* Correct the resultant for negative x values */
	if (x-dx < 0.0) {
		if (y-dy > 0.0)
			coord->lo += 180.0;
		else
			coord->lo -= 180.0;
	}
}

static void wgs84_to_kkj(struct gps_coord *coord)
{
	double la, lo;
	double d_la, d_lo;

	la = coord->la;
	lo = coord->lo;
	d_la = (-0.124766E+01 + 0.269941E+00 * la +
		-0.191342E+00 * lo + -0.356086E-02 * la * la +
		0.122353E-02 * la * lo + 0.335456E-03 * lo * lo) / 3600.0;
	d_lo = (0.286008E+02 + -0.114139E+01 * la +
		0.581329E+00 * lo + 0.152376E-01 * la * la +
		-0.118166E-01 * la * lo + -0.826201E-03 * lo * lo) / 3600.0;
	coord->la = la + d_la;
	coord->lo = lo + d_lo;
}

static void kkj_to_wgs84(struct gps_coord *coord)
{
	double la, lo;
	double d_la, d_lo;

	la = coord->la;
	lo = coord->lo;
	d_la = (0.124867E+01 + -0.269982E+00 * la +
		0.191330E+00 * lo + 0.356119E-02 * la * la +
		-0.122312E-02 * la * lo + -0.335514E-03 * lo * lo) / 3600.0;
	d_lo = (-0.286111E+02 + 0.114183E+01 * la +
		-0.581428E+00 * lo + -0.152421E-01 * la * la +
		0.118177E-01 * la * lo + 0.826646E-03 * lo * lo) / 3600.0;
	coord->la = la + d_la;
	coord->lo = lo + d_lo;
}

void gpsnav_convert_datum(struct gps_coord *coord,
			  const struct gps_datum *from_dtm,
			  const struct gps_datum *to_dtm)
{
	struct gps_coord tmp;
	int from_idx, to_idx;

	tmp = *coord;
	if (from_dtm != NULL)
		from_idx = from_dtm - gps_datum_table;
	else
		from_idx = GPS_DATUM_WGS84;
	switch (from_idx) {
	case GPS_DATUM_WGS84:
		break;
	case GPS_DATUM_FINLAND_HAYFORD:
		kkj_to_wgs84(&tmp);
		break;
	default:
		molod_translate(&tmp, from_dtm, 0);
	}

	if (to_dtm != NULL)
		to_idx = to_dtm - gps_datum_table;
	else
		to_idx = GPS_DATUM_WGS84;
	switch (to_idx) {
	case GPS_DATUM_WGS84:
		break;
	case GPS_DATUM_FINLAND_HAYFORD:
		wgs84_to_kkj(&tmp);
		break;
	default:
		molod_translate(&tmp, to_dtm, 1);
	}
	*coord = tmp;
}

const struct gps_datum *gpsnav_find_datum(struct gpsnav *gpsnav, const char *name)
{
	int i;

	for (i = 0; gps_datum_table[i].name != NULL; i++) {
		const struct gps_datum *dtm = &gps_datum_table[i];

		if (strcasecmp(dtm->name, name) == 0)
			return dtm;
	}
	return NULL;
}
