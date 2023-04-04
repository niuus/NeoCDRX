/****************************************************************************
* Scale2X GC Implementation
*
* Only works with RGB565!!!
*
* Check http://scale2x.sourceforge.net for algorithm info and complete
* Scale2X package
*****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/****************************************************************************
* Init_Scale2X
*
* Build a colour LUT for RGB565
****************************************************************************/
void Init_Scale2X(void)
{
}

/****************************************************************************
* Scale2X
*
* This is the actual algorithm implemented for the GC
****************************************************************************/
void Scale2X(char *srcpix, int srcpitch, char *dstpix, int dstpitch,
	     int width, int height)
{
    u16 E0, E1, E2, E3, B, D, E, F, H;
    int looph, loopw;
    int t;

    for (looph = 0; looph < height; ++looph) {
	for (loopw = 0; loopw < width; ++loopw) {
		
	    t = looph * srcpitch;	
	    B = *(u16 *) (srcpix + (MAX(0, looph - 1) * srcpitch) +
			  (loopw << 1));
	    D = *(u16 *) (srcpix + t +
			  (MAX(0, loopw - 1)<<1));
	    E = *(u16 *) (srcpix + t + (loopw << 1));
	    F = *(u16 *) (srcpix + t +
			  (2 * MIN(width - 1, loopw + 1)));
	    H = *(u16 *) (srcpix + (MIN(height - 1, looph + 1) * srcpitch) +
			  (loopw << 1));

	    E0 = D == B && B != F && D != H ? D : E;
	    E1 = B == F && B != D && F != H ? F : E;
	    E2 = D == H && D != B && H != F ? D : E;
	    E3 = H == F && D != H && B != F ? F : E;

	    t = looph * dstpitch * 2;
	    *(u16 *) (dstpix + t + loopw * 2 * 2) = E0;
	    *(u16 *) (dstpix + t + (loopw * 2 + 1) * 2) =
		E1;
	    *(u16 *) (dstpix + (looph * 2 + 1) * dstpitch + loopw * 2 * 2) =
		E2;
	    *(u16 *) (dstpix + (looph * 2 + 1) * dstpitch +
		      (loopw * 2 + 1) * 2) = E3;
	}
    }

}
