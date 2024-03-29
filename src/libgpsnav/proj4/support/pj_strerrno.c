/*
** libproj -- library of cartographic projections
**
** Copyright (c) 2003   Gerald I. Evenden
*/
static const char
LIBPROJ_ID[] = "$Id: pj_strerrno.c,v 2.7 2005/03/10 01:31:58 gie Exp gie $";
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
** list of projection system pj_errno values
*/
#include <string.h>
static struct {
	int errnum;
	char *name;
} pj_err_list[] = {
	{-1,	"no arguments in initialization list"},
	{-2,	"no options found in 'init' file"},
	{-3,	"no colon in init= string"},
	{-4,	"projection not named"},
	{-5,	"unknown projection id"},
	{-6,	"effective eccentricity = 1."},
	{-7,	"unknown unit conversion id"},
	{-8,	"invalid boolean param argument"},
	{-9,	"unknown elliptical parameter name"},
	{-10,	"reciprocal flattening (1/f) = 0"},
	{-11,	"|radius reference latitude| > 90"},
	{-12,	"squared eccentricity < 0"},
	{-13,	"major axis or radius = 0 or not given"},
	{-14,	"latitude or longitude exceeded limits"},
	{-15,	"invalid x or y"},
	{-16,	"improperly formed DMS value"},
	{-17,	"non-convergent inverse meridinal dist"},
	{-18,	"non-convergent inverse phi2"},
	{-19,	"acos/asin: |arg| >1.+1e-14"},
	{-20,	"tolerance condition error"},
	{-21,	"conic lat_1 = -lat_2"},
	{-22,	"lat_1 >= 90"},
	{-23,	"lat_1 = 0"},
	{-24,	"lat_ts >= 90"},
	{-25,	"no distance between control points"},
	{-26,	"projection not selected to be rotated"},
	{-27,	"W <= 0 or M <= 0"},
	{-28,	"lsat not in 1-5 range"},
	{-29,	"path not in range"},
	{-30,	"h <= 0"},
	{-31,	"k <= 0"},
	{-32,	"lat_0 = 0 or 90 or alpha = 90"},
	{-33,	"lat_1=lat_2 or lat_1=0 or lat_2=90"},
	{-34,	"elliptical usage required"},
	{-35,	"invalid UTM zone number"},
	{-36,	"arg(s) out of range for Tcheby eval"},
	{-37,	"failed to find projection to be rotated"},
	{-38,	"failed to load NAD27-83 correction file"},
	{-39,	"both n & m must be spec'd and > 0"},
	{-40,	"n <= 0, n > 1 or not specified"},
	{-41,	"lat_1 or lat_2 not specified"},
	{-42,	"|lat_1| == |lat_2|"},
	{-43,	"lat_0 is pi/2 from mean lat"},
	{-44,	"lat_0 not specified"},
	{-45,	"lat_0 == 0"},
	{-46,	"lat_0 != 0"},
	{-47,	"compiled/linked ONLY with libproj4 and standard libaries"}, 
	{-48,	"failed to select tangent or sine mode"},
	{-49,	"failed to set both +p and +q"},
	{0,		"invalid projection system error number"}
};
	char *
pj_strerrno(int err) {
		
	if (err > 0)
		return strerror(err);
	else {
		int i;

		for (i = 0; pj_err_list[i].errnum < 0 &&
					(pj_err_list[i].errnum != err); ++i) ;
		return pj_err_list[i].name;
	}
}
/*
** $Log: pj_strerrno.c,v $
** Revision 2.7  2005/03/10 01:31:58  gie
** added -48, -49
**
** Revision 2.6  2004/12/13 20:54:57  gie
** added message for external library abscence
**
** Revision 2.5  2004/07/12 17:57:33  gie
** added error for 'geos'
**
** Revision 2.4  2003/07/09 16:36:30  gie
** added some error codes
**
** Revision 2.3  2003/06/15 16:07:36  gie
** corrected include to "string"
**
** Revision 2.2  2003/05/25 15:36:13  gie
** restructured error list--more robust and easier to update.
**
** Revision 2.1  2003/03/28 01:44:30  gie
** Initial
**
*/

