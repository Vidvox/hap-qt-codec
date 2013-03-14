/*
 HapComponent.r
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

#include "HapResCommon.h"

#define    kHapAlphaCodecFormatType    kHapAlphaCodecSubType
#define    kHapAlphaCodecFormatName    "Hap Alpha"

// These flags specify information about the capabilities of the component
// Works with 32-bit Pixel Maps
#define kHapAlphaCompressorFlags ( codecInfoDoes32 )
#define kHapAlphaDecompressorFlags ( codecInfoDoes32 )

// These flags specify the possible format of compressed data produced by the component
// and the format of compressed files that the component can handle during decompression
// The component can decompress from files at 24 and 32-bit depths
#define kHapAlphaFormatFlags    ( codecInfoDepth32 )

#define kHapAlphaNameResID 456

#define kHapAlphaDecompressorDescriptionResID 458

#define kHapAlphaCompressorDescriptionResID 459

// Component Description
resource 'cdci' (456) {
    kHapAlphaCodecFormatName,       // Type
    1,                              // Version
    1,                              // Revision level
    kCodecManufacturerType,         // Manufacturer
    kHapAlphaDecompressorFlags,     // Decompression Flags
    kHapAlphaCompressorFlags,       // Compression Flags
    kHapAlphaFormatFlags,           // Format Flags
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
resource 'STR ' (kHapAlphaNameResID) {
    kHapAlphaCodecFormatName
};

#if DECO_BUILD
resource 'thng' (456) {
    decompressorComponentType,              // Type
    kHapAlphaCodecFormatType,               // SubType
    kCodecManufacturerType,                 // Manufacturer
    0,                                      // - use componentHasMultiplePlatforms
    0,
    0,
    0,
    'STR ',                                 // Name Type
    kHapAlphaNameResID,                     // Name ID
    'STR ',                                 // Info Type
    kHapAlphaDecompressorDescriptionResID,  // Info ID
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
        kHapAlphaDecompressorFlags | cmpThreadSafe,
        'dlle',
        kCodecDecompressorEntryPointResID,
        platformPowerPCNativeEntryPoint,
        #endif
        #if TARGET_REZ_MAC_X86
        kHapAlphaDecompressorFlags | cmpThreadSafe,
        'dlle',
        kCodecDecompressorEntryPointResID,
        platformIA32NativeEntryPoint,
        #endif
        #endif
        
        #if TARGET_OS_WIN32
        kHapAlphaDecompressorFlags,
        'dlle',
        kCodecDecompressorEntryPointResID,
        platformWin32,
        #endif
    },
    'thnr', 456;
};

// Component Information
resource 'STR ' (kHapAlphaDecompressorDescriptionResID) {
    "Hap Alpha Decompressor"
};

#endif // DECO_BUILD

#if COMP_BUILD
resource 'thng' (458) {
    compressorComponentType,                // Type
    kHapAlphaCodecFormatType,               // SubType
    kCodecManufacturerType,                 // Manufacturer
    0,                                      // - use componentHasMultiplePlatforms
    0,
    0,
    0,
    'STR ',                                 // Name Type
    kHapAlphaNameResID,                     // Name ID
    'STR ',                                 // Info Type
    kHapAlphaCompressorDescriptionResID,    // Info ID
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
        kHapAlphaCompressorFlags | cmpThreadSafe,
        'dlle',
        kCodecCompressorEntryPointResID,
        platformPowerPCNativeEntryPoint,
        #endif
        #if TARGET_REZ_MAC_X86
        kHapAlphaCompressorFlags | cmpThreadSafe,
        'dlle',
        kCodecCompressorEntryPointResID,
        platformIA32NativeEntryPoint,
        #endif
        #endif
        
        #if TARGET_OS_WIN32
        kHapAlphaCompressorFlags,
        'dlle',
        kCodecCompressorEntryPointResID,
        platformWin32,
        #endif
    },
    'thnr', 456;
};

// Component Information
resource 'STR ' (kHapAlphaCompressorDescriptionResID) {
    "Hap Alpha Compressor"
};

resource 'thnr' (456) {
    {
        'cdci', 1, 0, 'cdci', 456, cmpResourceNoFlags,
        'cpix', 1, 0, 'cpix', 456, cmpResourceNoFlags,
    }
};

resource 'cpix' (456) {
    {
        'DXT5',
        'RGBA',
        'BGRA'
    }
};

#endif // COMP_BUILD
