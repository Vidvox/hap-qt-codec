/*
 YCoCgDXT.h
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
      - Revert Chris Sidhall's C++ function overloading for C
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
#ifndef HapCodec_YCoCgDXT_h
#define HapCodec_YCoCgDXT_h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef byte
typedef unsigned char   byte;
#endif

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
int CompressYCoCgDXT5( const byte *inBuf, byte *outBuf, const int width, const int height , const int stride);

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
int DeCompressYCoCgDXT5( const byte *inBuf, byte *outBuf, const int width, const int height, const int stride );

#ifdef __cplusplus
}
#endif

#endif
