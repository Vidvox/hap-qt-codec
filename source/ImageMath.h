//
//  ImageMath.h
//
//  Created by Tom Butterworth on 02/08/2012.
//  Copyright (c) 2012 Tom Butterworth. All rights reserved.
//
//  Cross-platform image matrix-multiplication

#ifndef Pxlz_ImageMath_h
#define Pxlz_ImageMath_h

#include <stdint.h>
#include <stddef.h>

void ImageMath_MatrixMultiply8888(const void *src,
                                  size_t src_bytes_per_row,
                                  void *dst,
                                  size_t dst_bytes_per_row,
                                  unsigned long width,
                                  unsigned long height,
                                  const int16_t matrix[4*4],
                                  int32_t divisor,          // Applied after the matrix op
                                  const int16_t	*pre_bias,	// An array of 4 int16_t or NULL, added before matrix op
                                  const int32_t *post_bias);	// An array of 4 int32_t or NULL, added after matrix op

#endif
