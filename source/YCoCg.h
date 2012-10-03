//
//  YCoCg.h
//
//  Created by Tom Butterworth on 12/07/2012.
//  Copyright (c) 2012 Tom Butterworth. All rights reserved.
//

#ifndef YCoCg_h
#define YCoCg_h

#include <stdint.h>
#include <stddef.h>

/*
 RGB(A) and BGR(A) to and from CoCg(A)Y
 */
void ConvertRGBAToCoCgAY8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertCoCgAY8888ToRGBA( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertBGRAToCoCgAY8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertCoCgAY8888ToBGRA( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertBGR_ToCoCg_Y8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertCoCg_Y8888ToBGR_( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertRGB_ToCoCg_Y8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertCoCg_Y8888ToRGB_( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );

/*
 RGB(A) and BGR(A) to and from CoYCg(A)
 */
void ConvertRGBAToCoYCgA8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertCoYCgA8888ToRGBA( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertBGRAToCoYCgA8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertCoYCgA8888ToBGRA( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertBGR_ToCoYCg_8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertCoYCg_8888ToBGR_( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertRGB_ToCoYCg_8888( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
void ConvertCoYCg_8888ToRGB_( uint8_t *src, uint8_t *dst, unsigned long width, unsigned long height, size_t src_rowbytes, size_t dst_rowbytes );
#endif
