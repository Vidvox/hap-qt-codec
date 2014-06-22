/*
 YCoCgDXT.c
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


    Based on code by J.M.P. van Waveren / id Software, Inc.
    and changes by Chris Sidhall / Electronic Arts

    My changes are trivial:
      - Remove dependencies on other EAWebKit files
      - Mark unexported functions as static
      - Refactor to eliminate use of a global variable
      - Correct spelling of NVIDIA_7X_HARDWARE_BUG_FIX macro
      - Remove single usage of an assert macro


 Copyright (C) 2009-2011 Electronic Arts, Inc.  All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 
 1.  Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 2.  Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 3.  Neither the name of Electronic Arts, Inc. ("EA") nor the names of
 its contributors may be used to endorse or promote products derived
 from this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY ELECTRONIC ARTS AND ITS CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL ELECTRONIC ARTS OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

///////////////////////////////////////////////////////////////////////////////
// BCImageCompressionEA.cpp
// File created by Chrs Sidhall
// Also please see Copyright (C) 2007 Id Software, Inc used in this file.
///////////////////////////////////////////////////////////////////////////////

#include "YCoCgDXT.h"
#include <string.h>
#include <stdlib.h>

/* ALWAYS_INLINE */
/* Derived from EAWebKit's AlwaysInline.h, losing some of its support for other compilers */

#ifndef ALWAYS_INLINE
#if (defined(__GNUC__) || defined(__clang__)) && !defined(DEBUG)
#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif defined(_MSC_VER) && defined(NDEBUG)
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE inline
#endif
#endif

// CSidhall Note: The compression code is directly from http://developer.nvidia.com/object/real-time-ycocg-dxt-compression.html
// It was missing some Emit functions but have tried to keep it as close as possible to the orignal version.
// Also removed some alpha handling which was never used and added a few overloaded functions (like ExtractBlock).


/*
 Real-Time YCoCg DXT Compression
 Copyright (C) 2007 Id Software, Inc.
 Written by J.M.P. van Waveren
 
 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 */

/*
 * This code was modified by Electronic Arts Inc Copyright � 2009
 */

#ifndef word
typedef unsigned short  word;
#endif 

#ifndef dword
typedef unsigned int    dword;
#endif

#define INSET_COLOR_SHIFT       4       // inset color bounding box
#define INSET_ALPHA_SHIFT       5       // inset alpha bounding box

#define C565_5_MASK             0xF8    // 0xFF minus last three bits
#define C565_6_MASK             0xFC    // 0xFF minus last two bits

#define NVIDIA_G7X_HARDWARE_BUG_FIX     // keep the colors sorted as: max, min

#if defined(__LITTLE_ENDIAN__) || defined(_WIN32)
#define EA_SYSTEM_LITTLE_ENDIAN
#endif

static ALWAYS_INLINE word ColorTo565( const byte *color ) {
    return ( ( color[ 0 ] >> 3 ) << 11 ) | ( ( color[ 1 ] >> 2 ) << 5 ) | ( color[ 2 ] >> 3 );
}

static ALWAYS_INLINE void EmitByte( byte b, byte **outData ) {
    (*outData)[0] = b;
    *outData += 1;
}

static ALWAYS_INLINE void EmitUInt( unsigned int s, byte **outData ){
    (*outData)[0] = ( s >>  0 ) & 255;
    (*outData)[1] = ( s >>  8 ) & 255;
    (*outData)[2] = ( s >>  16 ) & 255;
    (*outData)[3] = ( s >>  24 ) & 255;
    *outData += 4;
}

static ALWAYS_INLINE void EmitUShort( unsigned short s, byte **outData ){
    (*outData)[0] = ( s >>  0 ) & 255;
    (*outData)[1] = ( s >>  8 ) & 255;
    *outData += 2;
}

static ALWAYS_INLINE void ExtractBlock( const byte *inPtr, const int stride, byte *colorBlock ) {
    for ( int j = 0; j < 4; j++ ) {
        memcpy( &colorBlock[j*4*4], inPtr, 4*4 );
        inPtr += stride;
    }
}

// This box extract replicates the last rows and columns if the row or columns are not 4 texels aligned
// This is so we don't get random pixels which could affect the color interpolation
static void ExtractBlock( const byte *inPtr, const int stride, const int widthRemain, const int heightRemain, byte *colorBlock ) {
    int *pBlock32 = (int *) colorBlock;  // Since we are using ARGA, we assume 4 byte alignment is already being used
    int *pSource32 = (int*) inPtr; 
    
    int hIndex=0;
    for(int j =0; j < 4; j++) {
        int wIndex = 0;
        for(int i=0; i < 4; i++) {
            pBlock32[i] = pSource32[wIndex];    
            // Set up offset for next column source (keep existing if we are at the end)         
            if(wIndex < (widthRemain - 1)) {
                wIndex++;
            }
        }
        
        // Set up offset for next texel row source (keep existing if we are at the end)
        pBlock32 +=4;    
        if(hIndex < (heightRemain-1)) {
            pSource32 +=(stride >> 2);
            hIndex++;
        }
    }
}

static void GetMinMaxYCoCg( byte *colorBlock, byte *minColor, byte *maxColor ) {
    minColor[0] = minColor[1] = minColor[2] = minColor[3] = 255;
    maxColor[0] = maxColor[1] = maxColor[2] = maxColor[3] = 0;
    
    for ( int i = 0; i < 16; i++ ) {
        if ( colorBlock[i*4+0] < minColor[0] ) {
            minColor[0] = colorBlock[i*4+0];
        }
        if ( colorBlock[i*4+1] < minColor[1] ) {
            minColor[1] = colorBlock[i*4+1];
        }
        // Note: the alpha is not used so no point in checking for it
        //        if ( colorBlock[i*4+2] < minColor[2] ) {
        //            minColor[2] = colorBlock[i*4+2];
        //        }
        if ( colorBlock[i*4+3] < minColor[3] ) {
            minColor[3] = colorBlock[i*4+3];
        }
        if ( colorBlock[i*4+0] > maxColor[0] ) {
            maxColor[0] = colorBlock[i*4+0];
        }
        if ( colorBlock[i*4+1] > maxColor[1] ) {
            maxColor[1] = colorBlock[i*4+1];
        }
        // Note: the alpha is not used so no point in checking for it
        //        if ( colorBlock[i*4+2] > maxColor[2] ) {
        //            maxColor[2] = colorBlock[i*4+2];
        //        }
        if ( colorBlock[i*4+3] > maxColor[3] ) {
            maxColor[3] = colorBlock[i*4+3];
        }
    }
}


// EA/Alex Mole: abs isn't inlined and gets called a *lot* in this code :)
// Let's make us an inlined one!
static ALWAYS_INLINE int absEA( int liArg )
{
    return ( liArg >= 0 ) ? liArg : -liArg;
}


static void ScaleYCoCg( byte *colorBlock, byte *minColor, byte *maxColor ) {
    int m0 = absEA( minColor[0] - 128 );      // (the 128 is to center to color to grey (128,128) )
    int m1 = absEA( minColor[1] - 128 );
    int m2 = absEA( maxColor[0] - 128 );
    int m3 = absEA( maxColor[1] - 128 );
    
    if ( m1 > m0 ) m0 = m1;
    if ( m3 > m2 ) m2 = m3;
    if ( m2 > m0 ) m0 = m2;
    
    const int s0 = 128 / 2 - 1;
    const int s1 = 128 / 4 - 1;
    
    int mask0 = -( m0 <= s0 );
    int mask1 = -( m0 <= s1 );
    int scale = 1 + ( 1 & mask0 ) + ( 2 & mask1 );
    
    minColor[0] = ( minColor[0] - 128 ) * scale + 128;
    minColor[1] = ( minColor[1] - 128 ) * scale + 128;
    minColor[2] = ( scale - 1 ) << 3;
    
    maxColor[0] = ( maxColor[0] - 128 ) * scale + 128;
    maxColor[1] = ( maxColor[1] - 128 ) * scale + 128;
    maxColor[2] = ( scale - 1 ) << 3;
    
    for ( int i = 0; i < 16; i++ ) {
        colorBlock[i*4+0] = ( colorBlock[i*4+0] - 128 ) * scale + 128;
        colorBlock[i*4+1] = ( colorBlock[i*4+1] - 128 ) * scale + 128;
    }
}

static void InsetYCoCgBBox( byte *minColor, byte *maxColor ) {
    int inset[4];
    int mini[4];
    int maxi[4];
    
    inset[0] = ( maxColor[0] - minColor[0] ) - ((1<<(INSET_COLOR_SHIFT-1))-1);
    inset[1] = ( maxColor[1] - minColor[1] ) - ((1<<(INSET_COLOR_SHIFT-1))-1);
    inset[3] = ( maxColor[3] - minColor[3] ) - ((1<<(INSET_ALPHA_SHIFT-1))-1);
    
    mini[0] = ( ( minColor[0] << INSET_COLOR_SHIFT ) + inset[0] ) >> INSET_COLOR_SHIFT;
    mini[1] = ( ( minColor[1] << INSET_COLOR_SHIFT ) + inset[1] ) >> INSET_COLOR_SHIFT;
    mini[3] = ( ( minColor[3] << INSET_ALPHA_SHIFT ) + inset[3] ) >> INSET_ALPHA_SHIFT;
    
    maxi[0] = ( ( maxColor[0] << INSET_COLOR_SHIFT ) - inset[0] ) >> INSET_COLOR_SHIFT;
    maxi[1] = ( ( maxColor[1] << INSET_COLOR_SHIFT ) - inset[1] ) >> INSET_COLOR_SHIFT;
    maxi[3] = ( ( maxColor[3] << INSET_ALPHA_SHIFT ) - inset[3] ) >> INSET_ALPHA_SHIFT;
    
    mini[0] = ( mini[0] >= 0 ) ? mini[0] : 0;
    mini[1] = ( mini[1] >= 0 ) ? mini[1] : 0;
    mini[3] = ( mini[3] >= 0 ) ? mini[3] : 0;
    
    maxi[0] = ( maxi[0] <= 255 ) ? maxi[0] : 255;
    maxi[1] = ( maxi[1] <= 255 ) ? maxi[1] : 255;
    maxi[3] = ( maxi[3] <= 255 ) ? maxi[3] : 255;
    
    minColor[0] = ( mini[0] & C565_5_MASK ) | ( mini[0] >> 5 );
    minColor[1] = ( mini[1] & C565_6_MASK ) | ( mini[1] >> 6 );
    minColor[3] = mini[3];
    
    maxColor[0] = ( maxi[0] & C565_5_MASK ) | ( maxi[0] >> 5 );
    maxColor[1] = ( maxi[1] & C565_6_MASK ) | ( maxi[1] >> 6 );
    maxColor[3] = maxi[3];
}

static void SelectYCoCgDiagonal( const byte *colorBlock, byte *minColor, byte *maxColor ) {
    byte mid0 = ( (int) minColor[0] + maxColor[0] + 1 ) >> 1;
    byte mid1 = ( (int) minColor[1] + maxColor[1] + 1 ) >> 1;
    
    byte side = 0;
    for ( int i = 0; i < 16; i++ ) {
        byte b0 = colorBlock[i*4+0] >= mid0;
        byte b1 = colorBlock[i*4+1] >= mid1;
        side += ( b0 ^ b1 );
    }
    
    byte mask = -( side > 8 );
    
#ifdef NVIDIA_G7X_HARDWARE_BUG_FIX
    mask &= -( minColor[0] != maxColor[0] );
#endif
    
    byte c0 = minColor[1];
    byte c1 = maxColor[1];
    
    // PlayStation 3 compiler warning fix:    
    // c0 ^= c1 ^= mask &= c0 ^= c1;    // Orignial code
    byte c2 = c0 ^ c1;
    c0 = c2;
    c0 ^= c1 ^= mask &=c2;
    
    minColor[1] = c0;
    maxColor[1] = c1;
}

static void EmitAlphaIndices( const byte *colorBlock, const byte minAlpha, const byte maxAlpha, byte **outData ) {
        
    const int ALPHA_RANGE = 7;
    
    byte mid, ab1, ab2, ab3, ab4, ab5, ab6, ab7;
    byte indexes[16];
    
    mid = ( maxAlpha - minAlpha ) / ( 2 * ALPHA_RANGE );
    
    ab1 = minAlpha + mid;
    ab2 = ( 6 * maxAlpha + 1 * minAlpha ) / ALPHA_RANGE + mid;
    ab3 = ( 5 * maxAlpha + 2 * minAlpha ) / ALPHA_RANGE + mid;
    ab4 = ( 4 * maxAlpha + 3 * minAlpha ) / ALPHA_RANGE + mid;
    ab5 = ( 3 * maxAlpha + 4 * minAlpha ) / ALPHA_RANGE + mid;
    ab6 = ( 2 * maxAlpha + 5 * minAlpha ) / ALPHA_RANGE + mid;
    ab7 = ( 1 * maxAlpha + 6 * minAlpha ) / ALPHA_RANGE + mid;
    
    for ( int i = 0; i < 16; i++ ) {
        byte a = colorBlock[i*4+3]; // Here it seems to be using the Y (luna) for the alpha
        int b1 = ( a <= ab1 );
        int b2 = ( a <= ab2 );
        int b3 = ( a <= ab3 );
        int b4 = ( a <= ab4 );
        int b5 = ( a <= ab5 );
        int b6 = ( a <= ab6 );
        int b7 = ( a <= ab7 );
        int index = ( b1 + b2 + b3 + b4 + b5 + b6 + b7 + 1 ) & 7;
        indexes[i] = index ^ ( 2 > index );
    }
    
    EmitByte( (indexes[ 0] >> 0) | (indexes[ 1] << 3) | (indexes[ 2] << 6),  outData );
    EmitByte( (indexes[ 2] >> 2) | (indexes[ 3] << 1) | (indexes[ 4] << 4) | (indexes[ 5] << 7), outData );
    EmitByte( (indexes[ 5] >> 1) | (indexes[ 6] << 2) | (indexes[ 7] << 5), outData );
    
    EmitByte( (indexes[ 8] >> 0) | (indexes[ 9] << 3) | (indexes[10] << 6), outData );
    EmitByte( (indexes[10] >> 2) | (indexes[11] << 1) | (indexes[12] << 4) | (indexes[13] << 7), outData );
    EmitByte( (indexes[13] >> 1) | (indexes[14] << 2) | (indexes[15] << 5), outData );
}

static void EmitColorIndices( const byte *colorBlock, const byte *minColor, const byte *maxColor, byte **outData ) {
    word colors[4][4];
    unsigned int result = 0;
    
    colors[0][0] = ( maxColor[0] & C565_5_MASK ) | ( maxColor[0] >> 5 );
    colors[0][1] = ( maxColor[1] & C565_6_MASK ) | ( maxColor[1] >> 6 );
    colors[0][2] = ( maxColor[2] & C565_5_MASK ) | ( maxColor[2] >> 5 );
    colors[0][3] = 0;
    colors[1][0] = ( minColor[0] & C565_5_MASK ) | ( minColor[0] >> 5 );
    colors[1][1] = ( minColor[1] & C565_6_MASK ) | ( minColor[1] >> 6 );
    colors[1][2] = ( minColor[2] & C565_5_MASK ) | ( minColor[2] >> 5 );
    colors[1][3] = 0;
    colors[2][0] = ( 2 * colors[0][0] + 1 * colors[1][0] ) / 3;
    colors[2][1] = ( 2 * colors[0][1] + 1 * colors[1][1] ) / 3;
    colors[2][2] = ( 2 * colors[0][2] + 1 * colors[1][2] ) / 3;
    colors[2][3] = 0;
    colors[3][0] = ( 1 * colors[0][0] + 2 * colors[1][0] ) / 3;
    colors[3][1] = ( 1 * colors[0][1] + 2 * colors[1][1] ) / 3;
    colors[3][2] = ( 1 * colors[0][2] + 2 * colors[1][2] ) / 3;
    colors[3][3] = 0;
    
    for ( int i = 15; i >= 0; i-- ) {
        int c0, c1;
        
        c0 = colorBlock[i*4+0];
        c1 = colorBlock[i*4+1];
        
        int d0 = absEA( colors[0][0] - c0 ) + absEA( colors[0][1] - c1 );
        int d1 = absEA( colors[1][0] - c0 ) + absEA( colors[1][1] - c1 );
        int d2 = absEA( colors[2][0] - c0 ) + absEA( colors[2][1] - c1 );
        int d3 = absEA( colors[3][0] - c0 ) + absEA( colors[3][1] - c1 );
        
        bool b0 = d0 > d3;
        bool b1 = d1 > d2;
        bool b2 = d0 > d2;
        bool b3 = d1 > d3;
        bool b4 = d2 > d3;
        
        int x0 = b1 & b2;
        int x1 = b0 & b3;
        int x2 = b0 & b4;
        
        int indexFinal =  ( x2 | ( ( x0 | x1 ) << 1 ) ) << ( i << 1 );
        result |= indexFinal;
    }
    
    EmitUInt( result, outData );
}

/*F*************************************************************************************************/
/*!
 \Function    CompressYCoCgDXT5( const byte *inBuf, byte *outBuf, const int width, const int height, const int stride ) 
 
 \Description        This is the C version of the YcoCgDXT5.  
 
 Input data needs to be converted from ARGB to YCoCg before calling this function.
 
 Does not support alpha at all since it uses the alpha channel to store the Y (luma).
 
 The output size is 4:1 but will be based on rounded up texture sizes on 4 texel boundaries 
 So for example if the source texture is 33 x 32, the compressed size will be 36x32. 
 
 The DXT5 compresses groups of 4x4 texels into 16 bytes (4:1 saving) 
 
 The compressed format:
 2 bytes of min and max Y luma values (these are used to rebuild an 8 element Luma table)
 6 bytes of indexes into the luma table
 3 bits per index so 16 indexes total 
 2 shorts of min and max color values (these are used to rebuild a 4 element chroma table)
 5 bits Co
 6 bits Cg
 5 bits Scale. The scale can only be 1, 2 or 4. 
 4 bytes of indexes into the Chroma CocG table 
 2 bits per index so 16 indexes total
 
 \Input              const byte *inBuf   Input buffer of the YCoCG textel data
 \Input              const byte *outBuf  Output buffer for the compressed data
 \Input              int width           in source width 
 \Input              int height          in source height
 \Input              int stride          in source in buffer stride in bytes
 
 \Output             int ouput size
 
 \Version    1.1     CSidhall 01/12/09 modified to account for non aligned textures
 1.2     1/10/10 Added stride
 */
/*************************************************************************************************F*/
extern "C" int CompressYCoCgDXT5( const byte *inBuf, byte *outBuf, const int width, const int height , const int stride) {
    
    int outputBytes =0;
    
    byte block[64];
    byte minColor[4];
    byte maxColor[4];
    
    byte *outData = outBuf;
    
    int blockLineSize = stride * 4;  // 4 lines per loop
    
    for ( int j = 0; j < height; j += 4, inBuf +=blockLineSize ) {
        int heightRemain = height - j;    
        for ( int i = 0; i < width; i += 4 ) {
            
            // Note: Modified from orignal source so that it can handle the edge blending better with non aligned 4x textures
            int widthRemain = width - i;
            if ((heightRemain < 4) || (widthRemain < 4) ) {
                ExtractBlock( inBuf + i * 4, stride, widthRemain, heightRemain,  block );  
            }
            else {
                ExtractBlock( inBuf + i * 4, stride, block );
            }
            // A simple min max extract for each color channel including alpha             
            GetMinMaxYCoCg( block, minColor, maxColor );
            ScaleYCoCg( block, minColor, maxColor );    // Sets the scale in the min[2] and max[2] offset
            InsetYCoCgBBox( minColor, maxColor );
            SelectYCoCgDiagonal( block, minColor, maxColor );
            
            EmitByte( maxColor[3], &outData );    // Note: the luma is stored in the alpha channel
            EmitByte( minColor[3], &outData );
            
            EmitAlphaIndices( block, minColor[3], maxColor[3], &outData );
            
            EmitUShort( ColorTo565( maxColor ), &outData );
            EmitUShort( ColorTo565( minColor ), &outData );
            
            EmitColorIndices( block, minColor, maxColor, &outData );
        }
    }
    
    outputBytes = (int)(outData - outBuf);
    
    return outputBytes;
}


//--- YCoCgDXT5 Decompression ---
static void RestoreLumaAlphaBlock(  const void * pSource, byte * colorBlock){
    byte *pS=(unsigned char *) pSource;
    byte luma[8];     
    
    // Grabbed this standard table building from undxt.cpp UnInterpolatedAlphaBlock() 
    luma[0] = *pS++;                                                      
    luma[1] = *pS++;                                                      
    luma[2] = (byte)((6 * luma[0] + 1 * luma[1] + 3) / 7);    
    luma[3] = (byte)((5 * luma[0] + 2 * luma[1] + 3) / 7);    
    luma[4] = (byte)((4 * luma[0] + 3 * luma[1] + 3) / 7);    
    luma[5] = (byte)((3 * luma[0] + 4 * luma[1] + 3) / 7);    
    luma[6] = (byte)((2 * luma[0] + 5 * luma[1] + 3) / 7);    
    luma[7] = (byte)((1 * luma[0] + 6 * luma[1] + 3) / 7);    
    
    int rawIndexes;
    int raw;
    int colorIndex=3;
    // We have 6 bytes of indexes (3 bits * 16 texels)
    // Easier to process in 2 groups of 8 texels... 
    for(int j=0; j < 2; j++) {
        // Pack the indexes so we can shift out the indexes as a group 
        rawIndexes = *pS++;
        raw = *pS++;
        rawIndexes |= raw << 8;
        raw = *pS++;
        rawIndexes |= raw << 16;  
        
        // Since we still have to operate on the texels, just store it in a linear array workspace     
        for(int i=0; i < 8; i++) {       
            static const int LUMA_INDEX_FILTER = 0x7;   // To isolate the 3 bit luma index
            
            byte index = (byte)(rawIndexes & LUMA_INDEX_FILTER);
            colorBlock[colorIndex] = luma[index];
            colorIndex += 4;                                            
            rawIndexes  >>=3;      
        }
    }   
}


// Converts a 5.6.5 short back into 3 bytes 
static ALWAYS_INLINE void Convert565ToColor( const unsigned short value , byte *pOutColor ) 
{
    int c = value >> (5+6);
    pOutColor[0] = c << 3;  // Was a 5 bit so scale back up 
    
    c = value >> 5;
    c &=0x3f;               // Filter out the top value
    pOutColor[1] = c << 2;  // Was a 6 bit 
    
    c = value & 0x1f;         // Filter out the top values
    pOutColor[2] = c << 3;  // was a 5 bit so scale back up 
}

#ifndef EA_SYSTEM_LITTLE_ENDIAN
// Flip around the 2 bytes in a short
static ALWAYS_INLINE short ShortFlipBytes( short raw ) 
{
    return ((raw >> 8) & 0xff) | (raw << 8);
}
#endif

static void RestoreChromaBlock( const void * pSource, byte *colorBlock)
{
    unsigned short *pS =(unsigned short *) pSource;
    pS +=4;  // Color info stars after 8 bytes (first 8 is the Y/alpha channel info)
    
    unsigned short rawColor = *pS++;     
#ifndef EA_SYSTEM_LITTLE_ENDIAN  
    rawColor = ShortFlipBytes(rawColor);
#endif
    
    
    byte color[4][4];   // Color workspace 
    
    // Build the color lookup table 
    // The luma should have already been extracted and sitting at offset[3]
    Convert565ToColor( rawColor , &color[0][0] );     
    rawColor = *pS++;     
#ifndef EA_SYSTEM_LITTLE_ENDIAN  
    rawColor = ShortFlipBytes(rawColor);
#endif
    
    Convert565ToColor( rawColor , &color[1][0] ); 
    
    // EA/Alex Mole: mixing float & int operations is horrifyingly slow on some platforms, so we do it different!
#if defined(__PPU__) || defined(_XBOX)
    color[2][0] = (byte) ( ( ((int)color[0][0] * 3) + ((int)color[1][0]    ) ) >> 2 );
    color[2][1] = (byte) ( ( ((int)color[0][1] * 3) + ((int)color[1][1]    ) ) >> 2 );
    color[3][0] = (byte) ( ( ((int)color[0][0]    ) + ((int)color[1][0] * 3) ) >> 2 );
    color[3][1] = (byte) ( ( ((int)color[0][1]    ) + ((int)color[1][1] * 3) ) >> 2 );
#else
    color[2][0] = (byte) ( (color[0][0] * 0.75f) + (color[1][0] * 0.25f) );
    color[2][1] = (byte) ( (color[0][1] * 0.75f) + (color[1][1] * 0.25f) );
    color[3][0] = (byte) ( (color[0][0] * 0.25f) + (color[1][0] * 0.75f) );
    color[3][1] = (byte) ( (color[0][1] * 0.25f) + (color[1][1] * 0.75f) );
#endif
    
    byte scale = ((color[0][2] >> 3) + 1) >> 1; // Adjust for shifts instead of divide
    
    // Scale back values here so we don't have to do it for all 16 texels
    // Note: This is really only for the software version.  In hardware, the scale would need to be restored during the YCoCg to RGB conversion.
    for(int i=0; i < 4; i++) {
        color[i][0] = ((color[i][0] - 128) >> scale) + 128;
        color[i][1] = ((color[i][1] - 128) >> scale) + 128;
    }
    
    // Rebuild the color block using the indexes (2 bits per texel)
    int rawIndexes;
    int colorIndex=0;
    
    // We have 2 shorts of indexes (2 bits * 16 texels = 32 bits). (If can confirm 4x alignment, can grab it as a word with single loop) 
    for(int j=0; j < 2; j++) {
        rawIndexes = *pS++;
#ifndef EA_SYSTEM_LITTLE_ENDIAN  
        rawIndexes = ShortFlipBytes(rawIndexes);
#endif
        
        // Since we still have to operate on block, just store it in a linear array workspace     
        for(int i=0; i < 8; i++) {       
            static const int COCG_INDEX_FILTER = 0x3;   // To isolate the 2 bit chroma index
            
            unsigned char index = (unsigned char)(rawIndexes & COCG_INDEX_FILTER);
            colorBlock[colorIndex] = color[index][0];
            colorBlock[colorIndex+1] = color[index][1];
            colorBlock[colorIndex+2] = 255;  
            colorIndex += 4;                                            
            rawIndexes  >>=2;      
        }
    }   
}

// This stores a 4x4 texel block but can overflow the output rectangle size if it is not 4 texels aligned in size
static int ALWAYS_INLINE StoreBlock( const byte *colorBlock, const int stride, byte *outPtr ) {
    
    for ( int j = 0; j < 4; j++ ) {
        memcpy( (void*) outPtr,&colorBlock[j*4*4], 4*4 );
        outPtr += stride;
    }
    return 64;
}

// This store only the texels that are within the width and height boundaries so does not overflow
static int StoreBlock( const byte *colorBlock , const int stride, const int widthRemain, const int heightRemain,  byte *outPtr) 
{
    int outCount =0;
    int width = stride >> 2;    // Convert to int offsets
    
    int *pBlock32 = (int *) colorBlock;  // Since we are using ARGB, we assume 4 byte alignment is already being used
    int *pOutput32 = (int*) outPtr; 
    
    int widthMax = 4;
    if(widthRemain < 4) {
        widthMax = widthRemain;
    }
    
    int heightMax = 4;
    if(heightRemain < 4) {
        heightMax = heightRemain;    
    }
    
    for(int j =0; j < heightMax; j++) {
        for(int i=0; i < widthMax; i++) {
            pOutput32[i] = pBlock32[i];    
            outCount +=4;       
        }
        
        // Set up offset for next texel row source (keep existing if we are at the end)
        pBlock32 +=4;    
        pOutput32 +=width;
    }
    return outCount;
}




/*F*************************************************************************************************/
/*!
 \Function    DeCompressYCoCgDXT5( const byte *inBuf, byte *outBuf, const int width, const int height, const int stride ) 
 
 \Description  Decompression for YCoCgDXT5  
 Bascially does the reverse order of he compression.  
 
 Ouptut data still needs to be converted from YCoCg to ARGB after this function has completed
 (probably more efficient to convert it inside here but have not done so to stay closer to the orginal
 sample code and just make it easier to follow).
 
 16 bytes get unpacked into a 4x4 texel block (64 bytes output).
 
 The compressed format:
 2 bytes of min and max Y luma values (these are used to rebuild an 8 element Luma table)
 6 bytes of indexes into the luma table
 3 bits per index so 16 indexes total 
 2 shorts of min and max color values (these are used to rebuild a 4 element chroma table)
 5 bits Co
 6 bits Cg
 5 bits Scale. The scale can only be 1, 2 or 4. 
 4 bytes of indexes into the Chroma CocG table 
 2 bits per index so 16 indexes total
 
 
 \Input          const byte *inBuf
 \Input          byte *outBuf, 
 \Input          const int width
 \input          const int height 
 \input          const int stride for inBuf
 
 \Output         int size output in bytes
 
 
 \Version    1.0        01/12/09 Created
 1.1        12/21/09 Alex Mole: removed branches from tight inner loop
 1.2        11/10/10 CSidhall: Added stride for textures with different image and canvas sizes.
 */
/*************************************************************************************************F*/
extern "C" int DeCompressYCoCgDXT5( const byte *inBuf, byte *outBuf, const int width, const int height, const int stride )
{
    byte colorBlock[64];    // 4x4 texel work space a linear array 
    int outByteCount =0;
    const byte *pCurInBuffer = inBuf;
    
    int blockLineSize = stride * 4;  // 4 lines per loop
    for( int j = 0; j < ( height & ~3 ); j += 4, outBuf += blockLineSize )
    {
        int i;
        for( i = 0; i < ( width & ~3 ); i += 4 )
        {
            RestoreLumaAlphaBlock(pCurInBuffer, colorBlock);
            RestoreChromaBlock(pCurInBuffer, colorBlock);           
            outByteCount += StoreBlock(colorBlock, stride, outBuf + i * 4);
            pCurInBuffer += 16; // 16 bytes per block of compressed data
        }
        
        // Do we have some leftover columns?
        if( width & 3 )
        {
            int widthRemain = width & 3;
            
            RestoreLumaAlphaBlock(pCurInBuffer, colorBlock);
            RestoreChromaBlock(pCurInBuffer, colorBlock);
            
            outByteCount += StoreBlock(colorBlock , stride, widthRemain, 4 /* heightRemain >= 4 */, outBuf + i * 4);
            
            pCurInBuffer += 16; // 16 bytes per block of compressed data
        }
    }
    
    // Do we have some leftover lines?
    if( height & 3 )
    {
        int heightRemain = height & 3;
        
        for( int i = 0; i < width; i += 4 )
        {
            RestoreLumaAlphaBlock(pCurInBuffer, colorBlock);
            RestoreChromaBlock(pCurInBuffer, colorBlock);
            
            int widthRemain = width - i;
            outByteCount += StoreBlock(colorBlock , stride, widthRemain, heightRemain,  outBuf + i * 4);
            
            pCurInBuffer += 16; // 16 bytes per block of compressed data
        }
    }
    
    return outByteCount;
}
