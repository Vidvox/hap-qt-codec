/*
 PixelFormats.c
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

#include "PixelFormats.h"
#if defined(__APPLE__)
#include <QuickTime/QuickTime.h>
#define HAP_BZERO(x,y) bzero((x),(y))
#else
#include <ConditionalMacros.h>
#include <ImageCodec.h>
#include <Windows.h>
#define HAP_BZERO(x,y) ZeroMemory((x),(y))
#endif
#include "Utility.h"

static void HapCodecAddBlockDescription(CFMutableDictionaryRef dictionary, unsigned int bits_per_pixel)
{
    // CV has a bug where it disregards kCVPixelFormatBlockHeight, so we lie about block size
    // (4x1 instead of actual 4x4) and add a vertical block-alignment key instead
    addNumberToDictionary(dictionary, kCVPixelFormatBitsPerBlock, bits_per_pixel * 4);
    addNumberToDictionary(dictionary, kCVPixelFormatBlockWidth, 4);
    addNumberToDictionary(dictionary, kCVPixelFormatBlockVerticalAlignment, 4);
}

static void HapCodecRegisterYCoCgDXTARGTC1PixelFormat()
{
    ICMPixelFormatInfo pixelInfo;

    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                            0,
                                                            &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    HAP_BZERO(&pixelInfo, sizeof(pixelInfo));
    pixelInfo.size  = sizeof(ICMPixelFormatInfo);
    pixelInfo.formatFlags = kICMPixelFormatHasAlphaChannel | 0x02; // two planes
    pixelInfo.bitsPerPixel[0] = 8;
    pixelInfo.bitsPerPixel[1] = 4;
    pixelInfo.cmpCount = 4;
    pixelInfo.cmpSize = 8 / 4;

    // Ignore any errors here as this could be a duplicate registration
    ICMSetPixelFormatInfo(kHapCVPixelFormat_YCoCg_DXT5_A_RGTC1, &pixelInfo);

    addNumberToDictionary(dict, kCVPixelFormatConstant, kHapCVPixelFormat_YCoCg_DXT5_A_RGTC1);

    CFMutableDictionaryRef planes[2];
    planes[0] = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    planes[1] = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    HapCodecAddBlockDescription(planes[0], 8);
    HapCodecAddBlockDescription(planes[1], 4);

    CFArrayRef array = CFArrayCreate(kCFAllocatorDefault, (const void **)planes, 2, &kCFTypeArrayCallBacks);

    CFRelease(planes[0]);
    CFRelease(planes[1]);

    CFDictionaryAddValue(dict, kCVPixelFormatPlanes, array);

    CFRelease(array);

    CFDictionarySetValue(dict, kCVPixelFormatOpenGLCompatibility, kCFBooleanTrue);

    // kCVPixelFormatContainsAlpha is only defined in the SDK for 10.7 plus
    CFDictionarySetValue(dict, CFSTR("ContainsAlpha"), kCFBooleanTrue);

    CVPixelFormatDescriptionRegisterDescriptionWithPixelFormatType(dict, kHapCVPixelFormat_YCoCg_DXT5_A_RGTC1);

    CFRelease(dict);
}

static void HapCodecRegisterDXTPixelFormat(OSType fmt, short bits_per_pixel, SInt32 open_gl_internal_format, Boolean has_alpha)
{
    /*
     * See http://developer.apple.com/legacy/mac/library/#qa/qa1401/_index.html
     */
    
    ICMPixelFormatInfo pixelInfo;
    
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                            0,
                                                            &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    HAP_BZERO(&pixelInfo, sizeof(pixelInfo));
    pixelInfo.size  = sizeof(ICMPixelFormatInfo);
    pixelInfo.formatFlags = (has_alpha ? kICMPixelFormatHasAlphaChannel : 0);
    pixelInfo.bitsPerPixel[0] = bits_per_pixel;
    pixelInfo.cmpCount = 4;
    pixelInfo.cmpSize = bits_per_pixel / 4;
    
    // Ignore any errors here as this could be a duplicate registration
    ICMSetPixelFormatInfo(fmt, &pixelInfo);
    
    addNumberToDictionary(dict, kCVPixelFormatConstant, fmt);
    
    HapCodecAddBlockDescription(dict, bits_per_pixel);
    
    addNumberToDictionary(dict, kCVPixelFormatOpenGLInternalFormat, open_gl_internal_format);
    
    CFDictionarySetValue(dict, kCVPixelFormatOpenGLCompatibility, kCFBooleanTrue);
    
    // kCVPixelFormatContainsAlpha is only defined in the SDK for 10.7 plus
    CFDictionarySetValue(dict, CFSTR("ContainsAlpha"), (has_alpha ? kCFBooleanTrue : kCFBooleanFalse));
    
    CVPixelFormatDescriptionRegisterDescriptionWithPixelFormatType(dict, fmt);
    CFRelease(dict);
}

static void HapCodecRegisterYCoCgPixelFormat(void)
{
    ICMPixelFormatInfo pixelInfo;
    
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                            0,
                                                            &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    HAP_BZERO(&pixelInfo, sizeof(pixelInfo));
    pixelInfo.size  = sizeof(ICMPixelFormatInfo);
    pixelInfo.formatFlags = 0;
    pixelInfo.bitsPerPixel[0] = 32;
    pixelInfo.cmpCount = 4;
    pixelInfo.cmpSize = 8;
    
    // Ignore any errors here as this could be a duplicate registration
    ICMSetPixelFormatInfo(kHapCVPixelFormat_CoCgXY, &pixelInfo);
    
    addNumberToDictionary(dict, kCVPixelFormatConstant, kHapCVPixelFormat_CoCgXY);
    addNumberToDictionary(dict, kCVPixelFormatBitsPerBlock, 32);
    
    // kCVPixelFormatContainsAlpha is only defined in the SDK for 10.7 plus
    CFDictionarySetValue(dict, CFSTR("ContainsAlpha"), kCFBooleanFalse);
    
    CVPixelFormatDescriptionRegisterDescriptionWithPixelFormatType(dict, kHapCVPixelFormat_CoCgXY);
    CFRelease(dict);
}

#if defined(WIN32)
static void HapCodecRegisterPixelFormats(void);

void HapCodecConstructor(void)
{
    HapCodecRegisterPixelFormats();
}

#else
__attribute__((constructor))
#endif
static void HapCodecRegisterPixelFormats(void)
{
    static Boolean registered = false;
    if (!registered)
    {
        // Register our DXT pixel buffer types if they're not already registered
        // arguments are: OSType, OpenGL internalFormat, alpha
        HapCodecRegisterDXTPixelFormat(kHapCVPixelFormat_RGB_DXT1, 4, 0x83F0, false);
        HapCodecRegisterDXTPixelFormat(kHapCVPixelFormat_RGBA_DXT5, 8, 0x83F3, true);
        HapCodecRegisterDXTPixelFormat(kHapCVPixelFormat_YCoCg_DXT5, 8, 0x83F3, false);
        HapCodecRegisterYCoCgPixelFormat();
        HapCodecRegisterYCoCgDXTARGTC1PixelFormat();
        registered = true;
    }
}
