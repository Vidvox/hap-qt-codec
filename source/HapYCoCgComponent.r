/*
 HapYCoCgCompressor.r
 Hap Codec
 
 Copyright (c) 2012-2013, Tom Butterworth and Vidvox LLC. All rights reserved.
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 * Neither the name of Hap nor the name of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.
 
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

#include "HapResCommon.h"

#define kHapYCoCgCodecFormatType kHapYCoCgCodecSubType
#define kHapYCoCgCodecFormatName "Hap Q"

// These flags specify information about the capabilities of the component
// Works with 32-bit Pixel Maps
#define kHapYCoCgCompressorFlags ( codecInfoDoes32 )
#define kHapYCoCgDecompressorFlags ( codecInfoDoes32 )

// These flags specify the possible format of compressed data produced by the component
// and the format of compressed files that the component can handle during decompression
// The component can decompress from files at 24-bit depth
#define kHapYCoCgFormatFlags ( codecInfoDepth24 )

#define kHapYCoCgNameResID 356

#define kHapYCoCgDecompressorDescriptionResID 358

#define kHapYCoCgCompressorDescriptionResID 359

// Component Description
resource 'cdci' (356) {
    kHapYCoCgCodecFormatName,       // Type
    1,                              // Version
    1,                              // Revision level
    kCodecManufacturerType,         // Manufacturer
    kHapYCoCgDecompressorFlags,     // Decompression Flags
    kHapYCoCgCompressorFlags,       // Compression Flags
    kHapYCoCgFormatFlags,           // Format Flags
    128,                            // Compression Accuracy
    128,                            // Decomression Accuracy
    200,                            // Compression Speed
    200,                            // Decompression Speed
    128,                            // Compression Level
    0,                              // Reserved
    1,                              // Minimum Height
    1,                              // Minimum Width
    0,                              // Decompression Pipeline Latency
    0,                              // Compression Pipeline Latency
    0                               // Private Data
};

// Component Name
resource 'STR ' (kHapYCoCgNameResID) {
    kHapYCoCgCodecFormatName
};

#if DECO_BUILD
resource 'thng' (356) {
    decompressorComponentType,              // Type
    kHapYCoCgCodecFormatType,               // SubType
    kCodecManufacturerType,                 // Manufacturer
    0,                                      // - use componentHasMultiplePlatforms
    0,
    0,
    0,
    'STR ',                                 // Name Type
    kHapYCoCgNameResID,                     // Name ID
    'STR ',                                 // Info Type
    kHapYCoCgDecompressorDescriptionResID,  // Info ID
    0,                                      // Icon Type
    0,                                      // Icon ID
    kHapDecompressorVersion,
    componentHasMultiplePlatforms +         // Registration Flags
        componentDoAutoVersion,
    0,                                      // Resource ID of Icon Family
    {    
#if TARGET_REZ_CARBON_MACHO
    #if !(TARGET_REZ_MAC_PPC || TARGET_REZ_MAC_X86)
        #error "Platform architecture not defined, TARGET_REZ_MAC_PPC and/or TARGET_REZ_MAC_X86 must be defined!"
    #endif
    
    #if TARGET_REZ_MAC_PPC
        kHapYCoCgDecompressorFlags | cmpThreadSafe,
        'dlle',
        kCodecDecompressorEntryPointResID,
        platformPowerPCNativeEntryPoint,
    #endif
    #if TARGET_REZ_MAC_X86
        kHapYCoCgDecompressorFlags | cmpThreadSafe,
        'dlle',
        kCodecDecompressorEntryPointResID,
        platformIA32NativeEntryPoint,
    #endif
#endif

#if TARGET_OS_WIN32
    kHapYCoCgDecompressorFlags, 
    'dlle',
    kCodecDecompressorEntryPointResID,
    platformWin32,
#endif
    },
    'thnr', 356;
};

// Component Information
resource 'STR ' (kHapYCoCgDecompressorDescriptionResID) {
    "Hap Q Decompressor"
};

#endif // DECO_BUILD

#if COMP_BUILD
resource 'thng' (358) {
    compressorComponentType,                // Type
    kHapYCoCgCodecFormatType,               // SubType
    kCodecManufacturerType,                 // Manufacturer
    0,                                      // - use componentHasMultiplePlatforms
    0,
    0,
    0,
    'STR ',                                 // Name Type
    kHapYCoCgNameResID,                     // Name ID
    'STR ',                                 // Info Type
    kHapYCoCgCompressorDescriptionResID,    // Info ID
    0,                                      // Icon Type
    0,                                      // Icon ID
    kHapCompressorVersion,
    componentHasMultiplePlatforms +         // Registration Flags
        componentDoAutoVersion,
    0,                                      // Resource ID of Icon Family
    {
#if TARGET_REZ_CARBON_MACHO
    #if !(TARGET_REZ_MAC_PPC || TARGET_REZ_MAC_X86)
        #error "Platform architecture not defined, TARGET_REZ_MAC_PPC and/or TARGET_REZ_MAC_X86 must be defined!"
    #endif
    
    #if TARGET_REZ_MAC_PPC    
        kHapYCoCgCompressorFlags | cmpThreadSafe,
        'dlle',
        kCodecCompressorEntryPointResID,
        platformPowerPCNativeEntryPoint,
    #endif
    #if TARGET_REZ_MAC_X86
        kHapYCoCgCompressorFlags | cmpThreadSafe,
        'dlle',
        kCodecCompressorEntryPointResID,
        platformIA32NativeEntryPoint,
    #endif
#endif

#if TARGET_OS_WIN32
    kHapYCoCgCompressorFlags,
    'dlle',
    kCodecCompressorEntryPointResID,
    platformWin32,
#endif
    },
    'thnr', 356;
};

// Component Information
resource 'STR ' (kHapYCoCgCompressorDescriptionResID) {
    "Hap Q Compressor"
};

resource 'thnr' (356) {
    {
        'cdci', 1, 0, 'cdci', 356, cmpResourceNoFlags,
        'cpix', 1, 0, 'cpix', 356, cmpResourceNoFlags,
        'ccop', 1, 0, 'ccop', 356, cmpResourceNoFlags,
    }
};

resource 'ccop' (356) {
    kCodecCompressionNoQuality;
};

// Pixel formats we accept and emit (we only accept CCXY, not emit it)
resource 'cpix' (356) {
    {
        'DYt5',
        'CCXY',
        'RGBA',
        'BGRA'
    }
};

#endif // COMP_BUILD
