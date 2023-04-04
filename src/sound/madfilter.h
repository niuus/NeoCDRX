/****************************************************************************
*   NeoCDRX
*   NeoGeo CD Emulator
*   NeoCD Redux - Copyright (C) 2007 softdev
****************************************************************************/

#ifndef __MADPLAYFILTER__
#define __MADPLAYFILTER__

struct audio_dither {
	mad_fixed_t error[3];
	mad_fixed_t random;
	unsigned long clipped_samples;
	mad_fixed_t peak_clipping;
	mad_fixed_t peak_sample;	
};

s32 audio_linear_dither( mad_fixed_t sample, struct audio_dither *dither );
	
#endif
