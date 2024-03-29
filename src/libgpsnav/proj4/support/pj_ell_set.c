/*
** libproj -- library of cartographic projections
**
** Copyright (c) 2003   Gerald I. Evenden
*/
static const char
LIBPROJ_ID[] = "$Id: pj_ell_set.c,v 2.2 2005/02/17 16:18:09 gie Exp gie $";
/*
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/*
** set ellipsoid parameters
*/
#include <lib_proj.h>
#include <string.h>
#define SIXTH .1666666666666666667 /* 1/6 */
#define RA4 .04722222222222222222 /* 17/360 */
#define RA6 .02215608465608465608 /* 67/3024 */
#define RV4 .06944444444444444444 /* 5/72 */
#define RV6 .04243827160493827160 /* 55/1296 */
	int /* initialize geographic shape parameters */
pj_ell_set(paralist *pl, double *a, double *es) {
	int i;
	double b = 0., e;
	const char *name;
	paralist *start = 0, *curr;

		/* check for varying forms of ellipsoid input */
	*a = *es = 0.;
	/* R takes precedence */
	if (pj_param(pl, "tR").i)
		*a = pj_param(pl, "dR").f;
	else { /* probable elliptical figure */

		/* check if ellps present and temporarily append its values to pl */
		if ((name = pj_param(pl, "sellps").s)) {
			char *s;

			for (start = pl; start && start->next ; start = start->next) ;
			curr = start;
			for (i = 0; (s = pj_ellps[i].id) && strcmp(name, s) ; ++i) ;
			if (!s) { pj_errno = -9; return 1; }
			curr = curr->next = pj_mkparam(pj_ellps[i].major);
			curr = curr->next = pj_mkparam(pj_ellps[i].ell);
		}
		/* set major axis to unity if not spec'd */
		*a = (pj_param(pl, "ta").i) ?  pj_param(pl, "da").f : 1.;
		if (pj_param(pl, "tes").i) /* eccentricity squared */
			*es = pj_param(pl, "des").f;
		else if (pj_param(pl, "te").i) { /* eccentricity */
			e = pj_param(pl, "de").f;
			*es = e * e;
		} else if (pj_param(pl, "trf").i) { /* recip flattening */
			*es = pj_param(pl, "drf").f;
			if (!*es) {
				pj_errno = -10;
				goto bomb;
			}
			*es = 1./ *es;
			*es = *es * (2. - *es);
		} else if (pj_param(pl, "tf").i) { /* flattening */
			*es = pj_param(pl, "df").f;
			*es = *es * (2. - *es);
		} else if (pj_param(pl, "tb").i) { /* minor axis */
			b = pj_param(pl, "db").f;
			*es = 1. - (b * b) / (*a * *a);
		}     /* else *es == 0. and sphere of radius *a */
		if (!b)
			b = *a * sqrt(1. - *es);
		/* following options turn ellipsoid into equivalent sphere */
		if (pj_param(pl, "bR_A").i) { /* sphere--area of ellipsoid */
			*a *= 1. - *es * (SIXTH + *es * (RA4 + *es * RA6));
			*es = 0.;
		} else if (pj_param(pl, "bR_V").i) { /* sphere--vol. of ellipsoid */
			*a *= 1. - *es * (SIXTH + *es * (RV4 + *es * RV6));
			*es = 0.;
		} else if (pj_param(pl, "bR_a").i) { /* sphere--arithmetic mean */
			*a = .5 * (*a + b);
			*es = 0.;
		} else if (pj_param(pl, "bR_g").i) { /* sphere--geometric mean */
			*a = sqrt(*a * b);
			*es = 0.;
		} else if (pj_param(pl, "bR_h").i) { /* sphere--harmonic mean */
			*a = 2. * *a * b / (*a + b);
			*es = 0.;
		} else if ((i = pj_param(pl, "tR_lat_a").i) || /* sphere--arith. */
			pj_param(pl, "tR_lat_g").i) { /* or geom. mean at latitude */
			double tmp;

			tmp = sin(pj_param(pl, i ? "rR_lat_a" : "rR_lat_g").f);
			if (fabs(tmp) > HALFPI) {
				pj_errno = -11;
				goto bomb;
			}
			tmp = 1. - *es * tmp * tmp;
			*a *= i ? .5 * (1. - *es + tmp) / ( tmp * sqrt(tmp)) :
				sqrt(1. - *es) / tmp;
			*es = 0.;
		}
bomb:
		if (start) { /* clean up temporary extension of list */
			free(start->next->next);
			free(start->next);
			start->next = 0;
		}
		if (pj_errno)
			return 1;
	}
	/* some remaining checks */
	if (*es < 0.)
		{ pj_errno = -12; return 1; }
	if (*a <= 0.)
		{ pj_errno = -13; return 1; }
	return 0;
}
/*
** $Log: pj_ell_set.c,v $
** Revision 2.2  2005/02/17 16:18:09  gie
** allowed for unit major axis/radius if not specified
**
** Revision 2.1  2003/03/28 01:44:30  gie
** Initial
**
*/
