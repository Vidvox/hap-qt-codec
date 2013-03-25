/*
 YCoCg.h
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

#ifndef YCoCg_h
#define YCoCg_h

/*
 Avoid a conflict with the QuickTime SDK and MSVC's non-standard stdint.h
*/
#if !defined(_STDINT) && !defined(_STDINT_H)
#include <stdint.h>
#endif
#include <stddef.h>

/*
 RGB(A) and BGR(A) to and from CoCg(A)Y
 */
void ConvertRGBAToCoCgAY8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertCoCgAY8888ToRGBA( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertBGRAToCoCgAY8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertCoCgAY8888ToBGRA( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertBGR_ToCoCg_Y8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertCoCg_Y8888ToBGR_( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertRGB_ToCoCg_Y8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertCoCg_Y8888ToRGB_( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );

/*
 RGB(A) and BGR(A) to and from CoYCg(A)
 */
void ConvertRGBAToCoYCgA8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertCoYCgA8888ToRGBA( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertBGRAToCoYCgA8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertCoYCgA8888ToBGRA( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertBGR_ToCoYCg_8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertCoYCg_8888ToBGR_( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertRGB_ToCoYCg_8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
void ConvertCoYCg_8888ToRGB_( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes, int allow_tile );
#endif
