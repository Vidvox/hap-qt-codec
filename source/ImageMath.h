/*
 ImageMath.h
 Hap Codec

 Copyright (c) 2012-2013, Tom Butterworth and Vidvox LLC. All rights reserved. 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//  Cross-platform image matrix-multiplication

#ifndef Pxlz_ImageMath_h
#define Pxlz_ImageMath_h

/*
 Avoid a conflict with the QuickTime SDK and MSVC's non-standard stdint.h
*/
#if !defined(_STDINT) && !defined(_STDINT_H)
#include <stdint.h>
#endif
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
                                  const int32_t *post_bias,	// An array of 4 int32_t or NULL, added after matrix op
                                  int allow_tile); // if non-zero, operation may be tiled and multithreaded

void ImageMath_Permute8888(const void *src,
                           size_t src_bytes_per_row,
                           void *dst,
                           size_t dst_bytes_per_row,
                           unsigned long width,
                           unsigned long height,
                           const uint8_t permuteMap[4], // positions are dst channel order, values are src channel order
                           int allow_tile); // if non-zero, operation may be tiled and multithreaded

#endif
