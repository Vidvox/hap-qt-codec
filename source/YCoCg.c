/*
 YCoCg.c
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

#include "YCoCg.h"
#include <stdint.h>
#include "ImageMath.h"

/*
 RGB <-> YCoCg
 Y  = [1/4  1/2  1/4][R]
 Co = [1/2  0   -1/2][G]
 Cg = [-1/4 1/2 -1/4][B]
 
 R  = [1    1   -1] [Y]
 G  = [1    0    1] [Co]
 B  = [1   -1   -1] [Cg]
 */

void ConvertRGBAToCoCgAY8888( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    const int32_t post_bias[4] = { 512, 512, 0, 0 };
    
    const int16_t matrix[16] = {
         2, -1,  0,  1, // R
         0,  2,  0,  2, // G
        -2, -1,  0,  1, // B
         0,  0,  4,  0  // A
    };
    
    ImageMath_MatrixMultiply8888(src, src_rowbytes, dst, dst_rowbytes, width, height, matrix, 4, NULL, post_bias, allow_tile);
}

void ConvertCoCgAY8888ToRGBA( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    const int16_t pre_bias[4] = { -128, -128, 0, 0 };
    
    const int16_t matrix[16] = {
         1,  0, -1,  0, // Co
        -1,  1, -1, 0,  // Cg
         0,  0,  0, 1,  // A
         1,  1,  1,  0  // Y
    };
    
    ImageMath_MatrixMultiply8888(src, src_rowbytes, dst, dst_rowbytes, width, height, matrix, 1, pre_bias, NULL, allow_tile);
}


void ConvertBGRAToCoCgAY8888( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    const int32_t post_bias[4] = { 512, 512, 0, 0 };
    
    const int16_t matrix[16] = {
        -2, -1,  0,  1, // B
         0,  2,  0,  2, // G
         2, -1,  0,  1, // R
         0,  0,  4,  0  // A
    };
    
    ImageMath_MatrixMultiply8888(src, src_rowbytes, dst, dst_rowbytes, width, height, matrix, 4, NULL, post_bias, allow_tile);
}

void ConvertCoCgAY8888ToBGRA( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    const int16_t pre_bias[4] = { -128, -128, 0, 0 };
    
    const int16_t matrix[16] = {
        -1,  0,  1,  0, // Co
        -1,  1, -1,  0, // Cg
         0,  0,  0,  1, // A
         1,  1,  1,  0  // Y
    };
    
    ImageMath_MatrixMultiply8888(src, src_rowbytes, dst, dst_rowbytes, width, height, matrix, 1, pre_bias, NULL, allow_tile);
}

void ConvertBGR_ToCoCg_Y8888( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    ConvertBGRAToCoCgAY8888(src, dst, width, height, src_rowbytes, dst_rowbytes, allow_tile);
}

void ConvertCoCg_Y8888ToBGR_( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    ConvertCoCgAY8888ToBGRA(src, dst, width, height, src_rowbytes, dst_rowbytes, allow_tile);
}

void ConvertRGB_ToCoCg_Y8888( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    ConvertRGBAToCoCgAY8888(src, dst, width, height, src_rowbytes, dst_rowbytes, allow_tile);
}

void ConvertCoCg_Y8888ToRGB_( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    ConvertCoCgAY8888ToRGBA(src, dst, width, height, src_rowbytes, dst_rowbytes, allow_tile);
}

void ConvertRGBAToCoYCgA8888( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    const int32_t post_bias[4] = { 512, 0, 512, 0 };

    const int16_t matrix[16] = {
         2,  1, -1,  0, // R
         0,  2,  2,  0, // G
        -2,  1, -1,  0, // B
         0,  0,  0,  4  // A
    };

    ImageMath_MatrixMultiply8888(src, src_rowbytes, dst, dst_rowbytes, width, height, matrix, 4, NULL, post_bias, allow_tile);
}

void ConvertCoYCgA8888ToRGBA( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    const int16_t pre_bias[4] = { -128, 0, -128, 0 };

    const int16_t matrix[16] = {
         1,  0, -1,  0, // Co
         1,  1,  1,  0, // Y
        -1,  1, -1,  0, // Cg
         0,  0,  0,  1  // A
    };

    ImageMath_MatrixMultiply8888(src, src_rowbytes, dst, dst_rowbytes, width, height, matrix, 1, pre_bias, NULL, allow_tile);
}

void ConvertBGRAToCoYCgA8888( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    const int32_t post_bias[4] = { 512, 0, 512, 0 };

    const int16_t matrix[16] = {
        -2,  1, -1,  0, // B
         0,  2,  2,  0, // G
         2,  1, -1,  0, // R
         0,  0,  0,  4  // A
    };

    ImageMath_MatrixMultiply8888(src, src_rowbytes, dst, dst_rowbytes, width, height, matrix, 4, NULL, post_bias, allow_tile);
}

void ConvertCoYCgA8888ToBGRA( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    const int16_t pre_bias[4] = { -128, 0, -128, 0 };

    const int16_t matrix[16] = {
        -1,  0,  1,  0, // Co
         1,  1,  1,  0, // Y
        -1,  1, -1,  0, // Cg
         0,  0,  0,  1  // A

    };

    ImageMath_MatrixMultiply8888(src, src_rowbytes, dst, dst_rowbytes, width, height, matrix, 1, pre_bias, NULL, allow_tile);
}

void ConvertBGR_ToCoYCg_8888( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    ConvertBGRAToCoYCgA8888(src, dst, width, height, src_rowbytes, dst_rowbytes, allow_tile);
}

void ConvertCoYCg_8888ToBGR_( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    ConvertCoYCgA8888ToBGRA(src, dst, width, height, src_rowbytes, dst_rowbytes, allow_tile);
}

void ConvertRGB_ToCoYCg_8888( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    ConvertRGBAToCoYCgA8888(src, dst, width, height, src_rowbytes, dst_rowbytes, allow_tile);
}

void ConvertCoYCg_8888ToRGB_( const uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile )
{
    ConvertCoYCgA8888ToRGBA(src, dst, width, height, src_rowbytes, dst_rowbytes, allow_tile);
}
