/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

/****************************************************************************
* NeoCDRX
*
* GX Video
****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include "neocdrx.h"

/*** External 2D Video ***/
extern u32 whichfb;
extern u32 *xfb[2];
extern GXRModeObj *vmode;

/*** 3D GX ***/
#define DEFAULT_FIFO_SIZE ( 256 * 1024 )
#define TEXSIZE ( (NEOSCR_WIDTH * NEOSCR_HEIGHT) * 2 )

static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);
static u8 texturemem[TEXSIZE] ATTRIBUTE_ALIGN (32);

GXTexObj texobj;
static Mtx view;
int vwidth, vheight, oldvwidth, oldvheight;

#define HASPECT 80
#define VASPECT 50

/* New texture based scaler */
typedef struct tagcamera
{
  guVector pos;
  guVector up;
  guVector view;
}
camera;

/*** Square Matrix
     This structure controls the size of the image on the screen.
	 Think of the output as a -80 x 80 by -60 x 60 graph.
***/
s16 square[] ATTRIBUTE_ALIGN (32) =
{
  /*
   * X,   Y,  Z
   * Values set are for roughly 4:3 aspect
   */
  -HASPECT, VASPECT, 0,		// 0
    HASPECT, VASPECT, 0,	// 1
    HASPECT, -VASPECT, 0,	// 2
    -HASPECT, -VASPECT, 0,	// 3
};

static camera cam = { {0.0F, 0.0F, 0.0F},
{0.0F, 0.5F, 0.0F},
{0.0F, 0.0F, -0.5F}
};

/****************************************************************************
 * Scaler Support Functions
 ****************************************************************************/
static void
draw_init (void)
{
  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
  GX_SetVtxDesc (GX_VA_CLR0, GX_INDEX8);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

  GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

  GX_SetNumTexGens (1);
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

  GX_InvalidateTexAll ();

  GX_InitTexObj (&texobj, texturemem, vwidth, vheight, GX_TF_RGB565,
		 GX_CLAMP, GX_CLAMP, GX_FALSE);
}

static void
draw_vert (u8 pos, u8 c, f32 s, f32 t)
{
  GX_Position1x8 (pos);
  GX_Color1x8 (c);
  GX_TexCoord2f32 (s, t);
}

static void
draw_square (Mtx v)
{
  Mtx m;			// model matrix.
  Mtx mv;			// modelview matrix.

  guMtxIdentity (m);
  guMtxTransApply (m, m, 0, 0, -100);
  guMtxConcat (v, m, mv);

  GX_LoadPosMtxImm (mv, GX_PNMTX0);
  GX_Begin (GX_QUADS, GX_VTXFMT0, 4);
  draw_vert (0, 0, 0.0, 0.0);
  draw_vert (1, 0, 1.0, 0.0);
  draw_vert (2, 0, 1.0, 1.0);
  draw_vert (3, 0, 0.0, 1.0);
  GX_End ();
}

/****************************************************************************
 * StartGX
 ****************************************************************************/
void
StartGX (void)
{
  Mtx p;

  GXColor gxbackground = { 0, 0, 0, 0xff };

	/*** Clear out FIFO area ***/
  memset (&gp_fifo, 0, DEFAULT_FIFO_SIZE);

	/*** Initialise GX ***/
  GX_Init (&gp_fifo, DEFAULT_FIFO_SIZE);
  GX_SetCopyClear (gxbackground, 0x00ffffff);

  GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
  GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
  GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
  GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);
  GX_SetFieldMode (vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  GX_SetCullMode (GX_CULL_NONE);
  GX_CopyDisp (xfb[whichfb ^ 1], GX_TRUE);
  GX_SetDispCopyGamma (GX_GM_1_0);

  guPerspective (p, 60, 1.33F, 10.0F, 1000.0F);
  GX_LoadProjectionMtx (p, GX_PERSPECTIVE);
  memset (texturemem, 0, TEXSIZE);
  vwidth = 100;
  vheight = 100;
}

/****************************************************************************
 * Update Video
 ****************************************************************************/
void
update_video (int width, int height, char *vbuffer)
{

  int h, w;
  long long int *dst = (long long int *) texturemem;
  long long int *src1 = (long long int *) vbuffer;
  long long int *src2 = (long long int *) (vbuffer + 640);
  long long int *src3 = (long long int *) (vbuffer + 1280);
  long long int *src4 = (long long int *) (vbuffer + 1920);

  vwidth = 320;
  vheight = 224;

  whichfb ^= 1;

  if ((oldvheight != vheight) || (oldvwidth != vwidth))
    {
		/** Update scaling **/
      oldvwidth = vwidth;
      oldvheight = vheight;
      draw_init ();

      memset (&view, 0, sizeof (Mtx));

		/*** Calling guLookAt seems to reboot to IPL! 
			I've moved the equivalent code below,
			and only call the C GU functions. ***/
      guLookAt (view, &cam.pos, &cam.up, &cam.view);
      GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);

    }

  GX_InvVtxCache ();
  GX_InvalidateTexAll ();
  GX_SetTevOp (GX_TEVSTAGE0, GX_DECAL);
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

  for (h = 0; h < vheight; h += 4)
    {
      for (w = 0; w < 80; w++)
	{
	  *dst++ = *src1++;
	  *dst++ = *src2++;
	  *dst++ = *src3++;
	  *dst++ = *src4++;
	}

      src1 += 240;
      src2 += 240;
      src3 += 240;
      src4 += 240;
    }

  DCFlushRange (texturemem, TEXSIZE);

  GX_SetNumChans (1);
  GX_LoadTexObj (&texobj, GX_TEXMAP0);

  draw_square (view);

  GX_DrawDone ();

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetColorUpdate (GX_TRUE);
  GX_CopyDisp (xfb[whichfb], GX_TRUE);
  GX_Flush ();

  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();

  VIDEO_WaitVSync ();

}
