/*
 * jdcolext.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * Copyright (C) 2009, 2011, D. R. Commander.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains output colorspace conversion routines.
 */


/* This file is included by jdcolor.c */


#if EV_OPTIMIZE

#ifndef FIRST
#define FIRST

static void get_start_end(j_decompress_ptr cinfo, int* start_col, int* end_col)
{
  if (!cinfo->n_crops) {
    *start_col = 0;
    *end_col = cinfo->output_width - 1;
    return;
  }
  int row = cinfo->output_scanline;

  int st = -1;
  int en = -2;
  int i;
  for (i = 0; i < cinfo->n_crops; i++) {
    if (row < cinfo->crops[i].y1 || row > cinfo->crops[i].y2) {
      continue;
    }
    if (st < 0) {
      st = cinfo->crops[i].x1;
      en = cinfo->crops[i].x2;
    }
    else {
      if (st > cinfo->crops[i].x1) {
	st = cinfo->crops[i].x1;
      }
      if (en < cinfo->crops[i].x2) {
	en = cinfo->crops[i].x2;
      }
    }
  }
  *start_col = st;
  *end_col = en;
}
#endif

#endif

/*
 * Convert some rows of samples to the output colorspace.
 *
 * Note that we change from noninterleaved, one-plane-per-component format
 * to interleaved-pixel format.  The output buffer is therefore three times
 * as wide as the input buffer.
 * A starting row offset is provided only for the input buffer.  The caller
 * can easily adjust the passed output_buf value to accommodate any row
 * offset required on that side.
 */


INLINE
LOCAL(void)
ycc_rgb_convert_internal (j_decompress_ptr cinfo,
                          JSAMPIMAGE input_buf, JDIMENSION input_row,
                          JSAMPARRAY output_buf, int num_rows)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  register int y, cb, cr;
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;
  /* copy these pointers into registers if possible */
  register JSAMPLE * range_limit = cinfo->sample_range_limit;
  register int * Crrtab = cconvert->Cr_r_tab;
  register int * Cbbtab = cconvert->Cb_b_tab;
  register INT32 * Crgtab = cconvert->Cr_g_tab;
  register INT32 * Cbgtab = cconvert->Cb_g_tab;
  SHIFT_TEMPS

    LOGM_S("ycc_rgb_convert_internal");

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
#if EV_OPTIMIZE
    int start_col = 0;
    int end_col = num_cols;
    get_start_end(cinfo, &start_col, &end_col);
    if (cinfo->crop_y_ptr && cinfo->crop_y_stride) {
      if (cinfo->n_crops) {
	int len = cinfo->crops[0].x2 - cinfo->crops[0].x1 + 1;
	if (len > cinfo->crop_y_stride) {
	  len = cinfo->crop_y_stride;
	}
	memcpy(cinfo->crop_y_ptr, inptr0 + cinfo->crops[0].x1, len);
	cinfo->crop_y_ptr += cinfo->crop_y_stride;
      }
      else {
	int len = num_cols;
	if (len > cinfo->crop_y_stride) {
	  len = cinfo->crop_y_stride;
	}
	memcpy(cinfo->crop_y_ptr, inptr0, len);
	cinfo->crop_y_ptr += cinfo->crop_y_stride;
      }
    }
#endif
    input_row++;
    outptr = *output_buf++;
#if EV_OPTIMIZE
    outptr += (start_col * RGB_PIXELSIZE);
    for (col = start_col; col <= end_col; col++)
#else
    for (col = 0; col < num_cols; col++)
#endif
      {
      y  = GETJSAMPLE(inptr0[col]);
      cb = GETJSAMPLE(inptr1[col]);
      cr = GETJSAMPLE(inptr2[col]);
      /* Range-limiting is essential due to noise introduced by DCT losses. */
      outptr[RGB_RED] =   range_limit[y + Crrtab[cr]];
      outptr[RGB_GREEN] = range_limit[y +
			      ((int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr],
						 SCALEBITS))];
      outptr[RGB_BLUE] =  range_limit[y + Cbbtab[cb]];
      /* Set unused byte to 0xFF so it can be interpreted as an opaque */
      /* alpha channel value */
#ifdef RGB_ALPHA
      outptr[RGB_ALPHA] = 0xFF;
#endif
      outptr += RGB_PIXELSIZE;
    }
  }
    LOGM_E("ycc_rgb_convert_internal");
}


/*
 * Convert grayscale to RGB: just duplicate the graylevel three times.
 * This is provided to support applications that don't want to cope
 * with grayscale as a separate case.
 */

INLINE
LOCAL(void)
gray_rgb_convert_internal (j_decompress_ptr cinfo,
                           JSAMPIMAGE input_buf, JDIMENSION input_row,
                           JSAMPARRAY output_buf, int num_rows)
{
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;
  LOGM("gray_rgb_convert_internal");

  while (--num_rows >= 0) {
    inptr = input_buf[0][input_row++];
    outptr = *output_buf++;
    for (col = 0; col < num_cols; col++) {
      /* We can dispense with GETJSAMPLE() here */
      outptr[RGB_RED] = outptr[RGB_GREEN] = outptr[RGB_BLUE] = inptr[col];
      /* Set unused byte to 0xFF so it can be interpreted as an opaque */
      /* alpha channel value */
#ifdef RGB_ALPHA
      outptr[RGB_ALPHA] = 0xFF;
#endif
      outptr += RGB_PIXELSIZE;
    }
  }
}
