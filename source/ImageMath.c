/*
 ImageMath.c
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

#include "ImageMath.h"

#ifdef __APPLE__
#include <Availability.h>
#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && defined(__MAC_OS_X_VERSION_MAX_ALLOWED)
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
#define IMAGE_MATH_USE_V_IMAGE
#endif
#if defined(IMAGE_MATH_USE_V_IMAGE) && __MAC_OS_X_VERSION_MIN_REQUIRED < 1040
#define IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
#endif
#endif
#endif

#ifdef IMAGE_MATH_USE_V_IMAGE
#include <Accelerate/Accelerate.h>
#endif

#define CLAMP_UINT8( x ) ( (x) < 0 ? (0) : ( (x) > 255 ? 255 : (x) ) )

void ImageMath_MatrixMultiply8888(const void *src,
                                  size_t src_bytes_per_row,
                                  void *dst,
                                  size_t dst_bytes_per_row,
                                  unsigned long width,
                                  unsigned long height,
                                  const int16_t matrix[4*4],
                                  int32_t divisor,          // Applied after the matrix op
                                  const int16_t	*pre_bias,	// An array of 4 int16_t or NULL, added before matrix op
                                  const int32_t *post_bias,
                                  int allow_tile)
{
#ifdef IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
    if (vImageMatrixMultiply_ARGB8888 != NULL)
    {
#endif // IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
#ifdef IMAGE_MATH_USE_V_IMAGE
        vImage_Buffer v_src = {
            (void *)src,
            height,
            width,
            src_bytes_per_row
        };
        
        vImage_Buffer v_dst = {
            dst,
            height,
            width,
            dst_bytes_per_row
        };
        
        vImageMatrixMultiply_ARGB8888(&v_src,
                                      &v_dst,
                                      matrix,
                                      divisor,
                                      pre_bias,
                                      post_bias,
                                      (allow_tile == 0 ? kvImageDoNotTile : kvImageNoFlags));
#endif // IMAGE_MATH_USE_V_IMAGE
#ifdef IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
    }
    else
    {
#endif // IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
#if !defined(IMAGE_MATH_USE_V_IMAGE) || defined(IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED)
        unsigned long y, x;
        const uint8_t *pixel_src = (const uint8_t *)src;
        uint8_t *pixel_dst = (uint8_t *)dst;
        size_t src_bytes_extra_per_row = src_bytes_per_row - (width * 4);
        size_t dst_bytes_extra_per_row = dst_bytes_per_row - (width * 4);

        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                
                int32_t result[4];
                int32_t source[4] = { pixel_src[0], pixel_src[1], pixel_src[2], pixel_src[3] };
                
                // Pre-bias
                if (pre_bias != NULL)
                {
                    source[0] += pre_bias[0];
                    source[1] += pre_bias[1];
                    source[2] += pre_bias[2];
                    source[3] += pre_bias[3];
                }
                
                // MM
                result[0] = (matrix[0] * source[0]) + (matrix[4] * source[1]) + (matrix[8]  * source[2]) + (matrix[12] * source[3]);
                result[1] = (matrix[1] * source[0]) + (matrix[5] * source[1]) + (matrix[9]  * source[2]) + (matrix[13] * source[3]);
                result[2] = (matrix[2] * source[0]) + (matrix[6] * source[1]) + (matrix[10] * source[2]) + (matrix[14] * source[3]);
                result[3] = (matrix[3] * source[0]) + (matrix[7] * source[1]) + (matrix[11] * source[2]) + (matrix[15] * source[3]);
                
                // Post-bias
                if (post_bias != NULL)
                {
                    result[0] += post_bias[0];
                    result[1] += post_bias[1];
                    result[2] += post_bias[2];
                    result[3] += post_bias[3];
                }
                
                // Divisor
                result[0] /= divisor;
                result[1] /= divisor;
                result[2] /= divisor;
                result[3] /= divisor;
                
                // Clamp
                pixel_dst[0] = CLAMP_UINT8(result[0]);
                pixel_dst[1] = CLAMP_UINT8(result[1]);
                pixel_dst[2] = CLAMP_UINT8(result[2]);
                pixel_dst[3] = CLAMP_UINT8(result[3]);

                pixel_src += 4;
                pixel_dst += 4;
            }
            pixel_src += src_bytes_extra_per_row;
            pixel_dst += dst_bytes_extra_per_row;
        }
#endif // !defined(IMAGE_MATH_USE_V_IMAGE) || defined(IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED)
#ifdef IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
    }
#endif // IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
}

void ImageMath_Permute8888(const void *src,
                           size_t src_bytes_per_row,
                           void *dst,
                           size_t dst_bytes_per_row,
                           unsigned long width,
                           unsigned long height,
                           const uint8_t permuteMap[4],
                           int allow_tile)
{
#ifdef IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
    if (vImagePermuteChannels_ARGB8888 != NULL)
    {
#endif // IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
#ifdef IMAGE_MATH_USE_V_IMAGE
        vImage_Buffer v_src = {
            (void *)src,
            height,
            width,
            src_bytes_per_row
        };
        
        vImage_Buffer v_dst = {
            dst,
            height,
            width,
            dst_bytes_per_row
        };
        
        vImagePermuteChannels_ARGB8888(&v_src, &v_dst, permuteMap, (allow_tile == 0 ? kvImageDoNotTile : kvImageNoFlags));
#endif // IMAGE_MATH_USE_V_IMAGE
#ifdef IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
    }
    else
    {
#endif // IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
#if !defined(IMAGE_MATH_USE_V_IMAGE) || defined(IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED)
        unsigned long y, x;
        int i;
        const uint8_t *pixel_src = (const uint8_t *)src;
        uint8_t *pixel_dst = (uint8_t *)dst;
        size_t src_bytes_extra_per_row = src_bytes_per_row - (width * 4);
        size_t dst_bytes_extra_per_row = dst_bytes_per_row - (width * 4);
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                for(i = 0; i < 4; i++ ) {
                    pixel_dst[i] = pixel_src[permuteMap[i]];
                }
                pixel_src += 4;
                pixel_dst += 4;
            }
            pixel_src += src_bytes_extra_per_row;
            pixel_dst += dst_bytes_extra_per_row;
        }
#endif // !defined(IMAGE_MATH_USE_V_IMAGE) || defined(IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED)
#ifdef IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
    }
#endif // IMAGE_MATH_USE_V_IMAGE_WEAK_LINKED
}
