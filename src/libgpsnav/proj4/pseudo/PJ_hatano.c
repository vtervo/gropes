/*
** libproj -- library of cartographic projections
**
** Copyright (c) 2003, 2005   Gerald I. Evenden
*/
static const char
LIBPROJ_ID[] = "$Id: PJ_hatano.c,v 2.2 2005/02/10 19:19:11 gie Exp gie $";
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
#define PJ_LIB__
#define PROJ_PARMS__\
	int	sym;
#include	<lib_proj.h>
PROJ_HEAD(hatano, "Hatano Equal Area") "\n\tPCyl, Sph.\n\[tsym=]";
#define NITER	20
#define EPS	1e-7
#define ONETOL 1.000001
#define CN	2.67595
#define CS	2.43763
#define RCN	0.37369906014686373063
#define RCS	0.41023453108141924738
#define FYCN	1.75859
#define FYCS	1.93052
#define RYCN	0.56863737426006061674
#define RYCS	0.51799515156538134803
#define FXC	0.85
#define RXC	1.17647058823529411764
FORWARD(s_forward); /* spheroid */
	double th1, c;
	int i, opt = !P->sym && (lp.phi < 0.);

	c = sin(lp.phi) * (opt ? CS : CN);
	for (i = NITER; i; --i) {
		lp.phi -= th1 = (lp.phi + sin(lp.phi) - c) / (1. + cos(lp.phi));
		if (fabs(th1) < EPS) break;
	}
	xy.x = FXC * lp.lam * cos(lp.phi *= .5);
	xy.y = sin(lp.phi) * (opt ? FYCS : FYCN);
	return (xy);
}
INVERSE(s_inverse); /* spheroid */
	double th;
	int opt = !P->sym && (xy.y < 0.);

	th = xy.y * ( opt ? RYCS : RYCN);
	if (fabs(th) > 1.)
		if (fabs(th) > ONETOL)	I_ERROR
		else			th = th > 0. ? HALFPI : - HALFPI;
	else
		th = asin(th);
	lp.lam = RXC * xy.x / cos(th);
	th += th;
	lp.phi = (th + sin(th)) * (opt ? RCS : RCN);
	if (fabs(lp.phi) > 1.)
		if (fabs(lp.phi) > ONETOL)	I_ERROR
		else			lp.phi = lp.phi > 0. ? HALFPI : - HALFPI;
	else
		lp.phi = asin(lp.phi);
	return (lp);
}
FREEUP; if (P) free(P); }
ENTRY0(hatano)
	P->sym = pj_param(P->params, "tsym").i;
	P->es = 0.;
	P->inv = s_inverse;
	P->fwd = s_forward;
ENDENTRY(P)
/*
** $Log: PJ_hatano.c,v $
** Revision 2.2  2005/02/10 19:19:11  gie
** added symmetric option
**
** Revision 2.1  2003/03/28 01:46:51  gie
** Initial
**
*/
