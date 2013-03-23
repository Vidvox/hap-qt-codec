/*
 HapCompressor.c
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

#if defined(__APPLE__)
#include <QuickTime/QuickTime.h>
#define kHapCodecCVPixelBufferLockFlags kCVPixelBufferLock_ReadOnly
#else
#include <ConditionalMacros.h>
#include <ImageCodec.h>
#include <malloc.h>
#define kHapCodecCVPixelBufferLockFlags 0
#endif

#include "HapPlatform.h"
#include "HapCodecVersion.h"
#include "Utility.h"
#include "PixelFormats.h"
#include "HapCodecSubTypes.h"
#include "hap.h"
#include "Lock.h"
#include "Tasks.h"
#include "Buffers.h"
#include "DXTEncoder.h"
#include "ImageMath.h"

/*
 On Apple we support GL encoding using the GPU.
 The GPU is very fast but produces low quality results
 Squish produces nicer results but takes longer.
 We select the GPU when the quality setting is above "High"
 YCoCg encodes YCoCg in DXT and requires a shader to draw, and produces very high quality results
 */

#if defined(__APPLE__)
#include "GLDXTEncoder.h"
#endif
#include "SquishEncoder.h"
#include "YCoCg.h"
#include "YCoCgDXTEncoder.h"

typedef struct HapCodecCompressTask HapCodecCompressTask;

typedef struct {
	ComponentInstance				self;
	ComponentInstance				target;
	
	ICMCompressorSessionRef 		session; // NOTE: we do not need to retain or release this
	
    OSType                          type;
    
	long							width;
	long							height;
	size_t							maxEncodedDataSize;
	
    CodecQ                          quality;
    
    int                             lastFrameOut;
    HapCodecBufferRef               finishedFrames;
    HapCodecLock                    lock;
    
    HapCodecBufferPoolRef           compressTaskPool;
    
    HapCodecDXTEncoderRef           dxtEncoder;
    
    HapCodecBufferPoolRef           formatConvertPool;
    size_t                          formatConvertBufferBytesPerRow;
    
    HapCodecBufferPoolRef           dxtBufferPool;
    
    unsigned int                    dxtFormat;
    HapCodecTaskGroupRef            taskGroup;
    
#ifdef DEBUG
    unsigned int                    debugFrameCount;
    uint64_t                        debugStartTime;
    uint64_t                        debugLastFrameTime;
    unsigned long                   debugTotalFrameBytes;
    unsigned long                   debugLargestFrameBytes;
    unsigned long                   debugSmallestFrameBytes;    
#endif
} HapCompressorGlobalsRecord, *HapCompressorGlobals;

struct HapCodecCompressTask
{
    HapCompressorGlobals            glob;
    ICMCompressorSourceFrameRef     sourceFrame;
    CVPixelBufferRef                sourceBuffer;
    ICMMutableEncodedFrameRef       encodedFrame;
    void *                          encodedFrameBaseAddress;
    unsigned long                   encodedFrameActualSize;
    ComponentResult                 error;
    HapCodecBufferRef               next; // Used to queue finished tasks
};

// Setup required for ComponentDispatchHelper.c
#define IMAGECODEC_BASENAME() 		Hap_C
#define IMAGECODEC_GLOBALS() 		HapCompressorGlobals storage

#define CALLCOMPONENT_BASENAME()	IMAGECODEC_BASENAME()
#define	CALLCOMPONENT_GLOBALS()		IMAGECODEC_GLOBALS()

#define QT_BASENAME()				CALLCOMPONENT_BASENAME()
#define QT_GLOBALS()				CALLCOMPONENT_GLOBALS()

#define COMPONENT_UPP_PREFIX()		uppImageCodec
#define COMPONENT_DISPATCH_FILE		"HapCompressorDispatch.h"
#define COMPONENT_SELECT_PREFIX()  	kImageCodec

#if __APPLE_CC__
#include <CoreServices/Components.k.h>
#include <QuickTime/ImageCodec.k.h>
#include <QuickTime/ImageCompression.k.h>
#include <QuickTime/ComponentDispatchHelper.c>
#else
#include <Components.k.h>
#include <ImageCodec.k.h>
#include <ImageCompression.k.h>
#include <ComponentDispatchHelper.c>
#endif

static void Background_Encode(void *info);
static void releaseTaskFrames(HapCodecCompressTask *task);
static ComponentResult finishFrame(HapCodecBufferRef buffer);
static void queueEncodedFrame(HapCompressorGlobals glob, HapCodecBufferRef frame);
static HapCodecBufferRef dequeueNextFrameOut(HapCompressorGlobals glob);

// Open a new instance of the component.
// Allocate component instance storage ("globals") and associate it with the new instance so that other
// calls will receive it.  
// Note that "one-shot" component calls like CallComponentVersion and ImageCodecGetCodecInfo work by opening
// an instance, making that call and then closing the instance, so you should avoid performing very expensive
// initialization operations in a component's Open function.
ComponentResult 
Hap_COpen(
  HapCompressorGlobals glob,
  ComponentInstance self )
{    
	ComponentResult err = noErr;
    ComponentDescription componentDescription;
#ifndef __clang_analyzer__
    glob = (HapCompressorGlobals)calloc( sizeof( HapCompressorGlobalsRecord ), 1 );
#endif
	if( ! glob ) {
		err = memFullErr;
		goto bail;
	}
	SetComponentInstanceStorage( self, (Handle)glob );
    
    // Store the type of compressor we are (Hap or Hap YCoCg)
    GetComponentInfo((Component)self, &componentDescription, 0, 0, 0);
    
	glob->self = self;
	glob->target = self;
    glob->session = NULL;
    glob->type = componentDescription.componentSubType;
    glob->width = 0;
    glob->height = 0;
	glob->maxEncodedDataSize = 0;
    glob->quality = codecNormalQuality;
    glob->lastFrameOut = 0;
    glob->finishedFrames = NULL;
    glob->lock = HAP_CODEC_LOCK_INIT;
    glob->compressTaskPool = NULL;
    glob->dxtEncoder = NULL;
    glob->formatConvertPool = NULL;
    glob->formatConvertBufferBytesPerRow = 0;
    glob->dxtBufferPool = NULL;
    glob->dxtFormat = 0;
    glob->taskGroup = NULL;
    
bail:
    debug_print_err(glob, err);
	return err;
}

// Closes the instance of the component.
// Release all storage associated with this instance.
// Note that if a component's Open function fails with an error, its Close function will still be called.
ComponentResult 
Hap_CClose(
  HapCompressorGlobals glob,
  ComponentInstance self HAP_ATTR_UNUSED)
{
    HapCodecBufferRef buffer;
    HapCodecBufferRef next;
	if( glob )
    {
#ifdef DEBUG
        if (glob->debugFrameCount)
        {
            char stringBuffer[255];
            uint64_t elapsed = glob->debugLastFrameTime - glob->debugStartTime;
            double time = (double)elapsed / CVGetHostClockFrequency();
            
            unsigned int uncompressed = glob->width * glob->height;
            
            if (glob->dxtFormat == HapTextureFormat_RGB_DXT1) uncompressed /= 2;
            uncompressed += 4U; // Hap uses 4 extra bytes

            sprintf(stringBuffer, "HAP CODEC: ");
            
            if (glob->dxtEncoder == NULL)
            {
                sprintf(stringBuffer + strlen(stringBuffer), "DXT frames in");
            }
            else if (glob->dxtEncoder->describe_function)
            {
                sprintf(stringBuffer + strlen(stringBuffer), "%s ", glob->dxtEncoder->describe_function(glob->dxtEncoder));
            }
            sprintf(stringBuffer + strlen(stringBuffer), "%u frames over %.1f seconds %.1f FPS. ", glob->debugFrameCount, time, glob->debugFrameCount / time);
            sprintf(stringBuffer + strlen(stringBuffer), "Largest frame bytes: %lu smallest: %lu average: %lu ",
                   glob->debugLargestFrameBytes, glob->debugSmallestFrameBytes, glob->debugTotalFrameBytes/ glob->debugFrameCount);
            sprintf(stringBuffer + strlen(stringBuffer), "uncompressed: %d\n", uncompressed);
            debug_print(glob, stringBuffer);
        }
#endif
        HapCodecBufferPoolDestroy(glob->dxtBufferPool);
        glob->dxtBufferPool = NULL;
        
        HapCodecDXTEncoderDestroy(glob->dxtEncoder);
        glob->dxtEncoder = NULL;
        
        HapCodecBufferPoolDestroy(glob->formatConvertPool);
        glob->formatConvertPool = NULL;
        
        HapCodecTasksWaitForGroupToComplete(glob->taskGroup);
        HapCodecTasksDestroyGroup(glob->taskGroup);
        glob->taskGroup = NULL;
        
        // We should never have queued frames when we are closed
        // but we check and properly release the memory if we do
        
        buffer = glob->finishedFrames;
        while (buffer) {
            HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress(buffer);
            if (task)
            {
                releaseTaskFrames(task);
                next = task->next;
            }
            else
            {
                next = NULL;
            }
            HapCodecBufferReturn(buffer);
            buffer = next;
        }
        
        HapCodecBufferPoolDestroy(glob->compressTaskPool);
        glob->compressTaskPool = NULL;
        
        HapCodecLockDestroy(&glob->lock);
        glob->lock = NULL;
        
		free( glob );
	}
	
	return noErr;
}

// Return the version of the component.
// This does not need to correspond in any way to the human-readable version numbers found in
// Get Info windows, etc.  
// The principal use of component version numbers is to choose between multiple installed versions
// of the same component: if the component manager sees two components with the same type, subtype
// and manufacturer codes and either has the componentDoAutoVersion registration flag set,
// it will deregister the one with the older version.  (If componentAutoVersionIncludeFlags is also 
// set, it only does this when the component flags also match.)
// By convention, the high short of the component version is the interface version, which Apple
// bumps when there is a major change in the interface.  
// We recommend bumping the low short of the component version every time you ship a release of a component.
// The version number in the 'thng' resource should be the same as the number returned by this function.
ComponentResult
Hap_CVersion(HapCompressorGlobals glob HAP_ATTR_UNUSED)
{
	return kHapCompressorVersion;
}

// Sets the target for a component instance.
// When a component wants to make a component call on itself, it should make that call on its target.
// This allows other components to delegate to the component.
// By default, a component's target is itself -- see the Open function.
ComponentResult 
Hap_CTarget(HapCompressorGlobals glob, ComponentInstance target)
{
	glob->target = target;
	return noErr;
}

// Your component receives the ImageCodecGetCodecInfo request whenever an application calls the Image Compression Manager's GetCodecInfo function.
// Your component should return a formatted compressor information structure defining its capabilities.
// Both compressors and decompressors may receive this request.
ComponentResult
Hap_CGetCodecInfo(HapCompressorGlobals glob, CodecInfo *info)
{
	OSErr err = noErr;

	if (info == NULL)
    {
		err = paramErr;
	}
	else
    {
		CodecInfo **tempCodecInfo;
        SInt16 resourceID = resourceIDForComponentType(glob->type, codecInfoResourceType);
        if (resourceID == 0)
        {
            err = internalComponentErr;
            goto bail;
        }
        
        err = GetComponentResource((Component)glob->self, codecInfoResourceType, resourceID, (Handle *)&tempCodecInfo);
        if (err == noErr)
        {
            *info = **tempCodecInfo;
            DisposeHandle((Handle)tempCodecInfo);
        }
    }
bail:
    debug_print_err(glob, err);
	return err;
}

// Return the maximum size of compressed data for the image in bytes.
// Note that this function is only used when the ICM client is using a compression sequence
// (created with CompressSequenceBegin, not ICMCompressionSessionCreate).
// Nevertheless, it's important to implement it because such clients need to know how much 
// memory to allocate for compressed frame buffers.
ComponentResult 
Hap_CGetMaxCompressionSize(
	HapCompressorGlobals glob HAP_ATTR_UNUSED,
	PixMapHandle        src HAP_ATTR_UNUSED,
	const Rect *        srcRect,
	short               depth HAP_ATTR_UNUSED,
	CodecQ              quality HAP_ATTR_UNUSED,
	long *              size)
{
    int dxtSize;
	if( ! size )
		return paramErr;
    dxtSize = dxtBytesForDimensions(srcRect->right - srcRect->left, srcRect->bottom - srcRect->top, glob->type);
	*size = HapMaxEncodedLength(dxtSize);
    
	return noErr;
}

// Create a dictionary that describes the kinds of pixel buffers that we want to receive.
// The important keys to add are kCVPixelBufferPixelFormatTypeKey,
// kCVPixelBufferWidthKey and kCVPixelBufferHeightKey.
// Many compressors will also want to set kCVPixelBufferExtendedPixels,
// kCVPixelBufferBytesPerRowAlignmentKey, kCVImageBufferGammaLevelKey and kCVImageBufferYCbCrMatrixKey.
static OSStatus
createPixelBufferAttributesDictionary( SInt32 width, SInt32 height,
                                      const OSType *pixelFormatList, int pixelFormatCount,
                                      Boolean padBuffers,
                                      CFMutableDictionaryRef *pixelBufferAttributesOut )
{
	OSStatus err = memFullErr;
	int i;
	CFMutableDictionaryRef pixelBufferAttributes = NULL;
	CFNumberRef number = NULL;
	CFMutableArrayRef array = NULL;
    SInt32 extendRight, extendBottom;
	
	pixelBufferAttributes = CFDictionaryCreateMutable( 
                                                      NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if( ! pixelBufferAttributes ) goto bail;
	
	array = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	if( ! array ) goto bail;
	
	// Under kCVPixelBufferPixelFormatTypeKey, add the list of source pixel formats.
	// This can be a CFNumber or a CFArray of CFNumbers.
	for( i = 0; i < pixelFormatCount; i++ ) {
		number = CFNumberCreate( NULL, kCFNumberSInt32Type, &pixelFormatList[i] );
		if( ! number ) goto bail;
		
		CFArrayAppendValue( array, number );
		
		CFRelease( number );
		number = NULL;
	}
	
	CFDictionaryAddValue( pixelBufferAttributes, kCVPixelBufferPixelFormatTypeKey, array );
	CFRelease( array );
	array = NULL;
	
	// Add kCVPixelBufferWidthKey and kCVPixelBufferHeightKey to specify the dimensions
	// of the source pixel buffers.  Normally this is the same as the compression target dimensions.
	addNumberToDictionary( pixelBufferAttributes, kCVPixelBufferWidthKey, width );
	addNumberToDictionary( pixelBufferAttributes, kCVPixelBufferHeightKey, height );
	
	// If we will use vImage it prefers 16-byte alignment - we set it always, but we could only set it if
    // the encoder needs it, however most buffers are 16-byte aligned anyway even without asking.
	addNumberToDictionary( pixelBufferAttributes, kCVPixelBufferBytesPerRowAlignmentKey, 16 );
	
    // If you want to require that extra scratch pixels be allocated on the edges of source pixel buffers,
	// add the kCVPixelBufferExtendedPixels{Left,Top,Right,Bottom}Keys to indicate how much.
	// (Note that if your compressor needs to copy the pixels anyhow (eg, in order to convert to a different
	// format) you may get better performance if your copy routine does not require extended pixels.)
    if (padBuffers)
    {
        extendRight = roundUpToMultipleOf4(width) - width;
        extendBottom = roundUpToMultipleOf4(height) - height;
        if(extendRight)
        {
            addNumberToDictionary(pixelBufferAttributes, kCVPixelBufferExtendedPixelsRightKey, extendRight);
        }
        if (extendBottom)
        {
            addNumberToDictionary(pixelBufferAttributes, kCVPixelBufferExtendedPixelsBottomKey, extendBottom);
        }
    }
    
	addDoubleToDictionary( pixelBufferAttributes, kCVImageBufferGammaLevelKey, 2.2 );
	
	err = noErr;
	*pixelBufferAttributesOut = pixelBufferAttributes;
	pixelBufferAttributes = NULL;
	
bail:
	if( pixelBufferAttributes ) CFRelease( pixelBufferAttributes );
	if( number ) CFRelease( number );
	if( array ) CFRelease( array );
	return err;
}

// Prepare to compress frames.
// Compressor should record session and sessionOptions for use in later calls.
// Compressor may modify imageDescription at this point.
// Compressor may create and return pixel buffer options.
ComponentResult 
Hap_CPrepareToCompressFrames(
  HapCompressorGlobals glob,
  ICMCompressorSessionRef session,
  ICMCompressionSessionOptionsRef sessionOptions,
  ImageDescriptionHandle imageDescription,
  void *reserved HAP_ATTR_UNUSED,
  CFDictionaryRef *compressorPixelBufferAttributesOut)
{
	ComponentResult err = noErr;
	CFMutableDictionaryRef compressorPixelBufferAttributes = NULL;
    OSType pixelFormatList[] = { k32BGRAPixelFormat, k32RGBAPixelFormat, 0, 0 };
    int pixelFormatCount;
	Fixed gammaLevel;
    long wantedDXTSize;
	
    switch (glob->type) {
        case kHapCodecSubType:
            pixelFormatList[2] = kHapCVPixelFormat_RGB_DXT1;
            pixelFormatCount = 3;
            break;
        case kHapAlphaCodecSubType:
            pixelFormatList[2] = kHapCVPixelFormat_RGBA_DXT5;
            pixelFormatCount = 3;
            break;
        case kHapYCoCgCodecSubType:
            pixelFormatList[2] = kHapCVPixelFormat_YCoCg_DXT5;
            pixelFormatList[3] = kHapCVPixelFormat_CoCgXY;
            pixelFormatCount = 4;
            break;
        default:
            err = internalComponentErr;
            goto bail;
    }
    
	// Record the compressor session for later calls to the ICM.
	// Note: this is NOT a CF type and should NOT be CFRetained or CFReleased.
	glob->session = session;
	
	// Modify imageDescription
    
	gammaLevel = kQTCCIR601VideoGammaLevel;
	err = ICMImageDescriptionSetProperty( imageDescription,
			kQTPropertyClass_ImageDescription,
			kICMImageDescriptionPropertyID_GammaLevel,
			sizeof(gammaLevel),
			&gammaLevel );
	if( err )
		goto bail;
	
	// Record the dimensions from the image description.
	glob->width = (*imageDescription)->width;
	glob->height = (*imageDescription)->height;
    
    if (glob->type == kHapAlphaCodecSubType)
        (*imageDescription)->depth = 32;
    else
        (*imageDescription)->depth = 24;
    
    // Record quality so we can select an appropriate DXT encoder
    if (glob->type == kHapCodecSubType || glob->type == kHapAlphaCodecSubType)
    {
        CodecQ quality;
        
        if(sessionOptions)
        {
            
            err = ICMCompressionSessionOptionsGetProperty(sessionOptions,
                                                          kQTPropertyClass_ICMCompressionSessionOptions,
                                                          kICMCompressionSessionOptionsPropertyID_Quality,
                                                          sizeof( quality ),
                                                          &quality,
                                                          NULL );
            if (err) goto bail;
        }
        else
        {
            quality = codecLosslessQuality;
        }
        
        glob->quality = quality;
        
        glob->dxtFormat = glob->type == kHapAlphaCodecSubType ? HapTextureFormat_RGBA_DXT5 : HapTextureFormat_RGB_DXT1;
    }
    else
    {
        glob->dxtFormat = HapTextureFormat_YCoCg_DXT5;
    }
    
    // Create a pixel buffer attributes dictionary.
    // Only the GL DXT encoder requires padded-to-4 source buffers
	err = createPixelBufferAttributesDictionary( glob->width, glob->height,
                                                pixelFormatList, pixelFormatCount,
                                                ((glob->type != kHapYCoCgCodecSubType && glob->quality < codecHighQuality) ? true : false),
                                                &compressorPixelBufferAttributes );
	if( err )
		goto bail;
    
	*compressorPixelBufferAttributesOut = compressorPixelBufferAttributes;
	compressorPixelBufferAttributes = NULL;
    
    // Work out the upper bound on encoded frame data size -- we'll allocate buffers of this size.
    wantedDXTSize = dxtBytesForDimensions(glob->width, glob->height, glob->type);
    
    glob->maxEncodedDataSize = HapMaxEncodedLength(wantedDXTSize);
    
    if (glob->compressTaskPool == NULL)
    {
        glob->compressTaskPool = HapCodecBufferPoolCreate(sizeof(HapCodecCompressTask));
    }
    
    if (glob->compressTaskPool == NULL)
    {
        err = memFullErr;
        goto bail;
    }
    
    glob->taskGroup = HapCodecTasksCreateGroup(Background_Encode, 20);
    
#ifdef DEBUG    
    glob->debugStartTime = CVGetCurrentHostTime();
#endif
bail:
	if( compressorPixelBufferAttributes ) CFRelease( compressorPixelBufferAttributes );
    debug_print_err(glob, err);
	return err;
}

static void Background_Encode(void *info)
{
    HapCodecBufferRef formatConvertBuffer = NULL;
    HapCodecBufferRef dxtBuffer = NULL;
    HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress((HapCodecBufferRef)info);
    HapCompressorGlobals glob = task->glob;
    
    const void *codec_src = NULL;
    unsigned int codec_src_length = 0;
    
    unsigned int sourceBytesPerRow = 0;
    OSType sourcePixelFormat = 0;
    void *sourceBaseAddress = NULL;
    
    OSType dxtPixelFormat = 0;
    
    unsigned int hapResult;
    
    ComponentResult err = noErr;
    
    if (CVPixelBufferLockBaseAddress(task->sourceBuffer, kHapCodecCVPixelBufferLockFlags) != kCVReturnSuccess)
    {
        task->sourceBuffer = NULL;
        err = internalComponentErr;
        goto bail;
    }
    
    sourceBytesPerRow = CVPixelBufferGetBytesPerRow(task->sourceBuffer);
    sourcePixelFormat = CVPixelBufferGetPixelFormatType(task->sourceBuffer);
    sourceBaseAddress = CVPixelBufferGetBaseAddress(task->sourceBuffer);
    
    dxtPixelFormat = (glob->type == kHapAlphaCodecSubType ? kHapCVPixelFormat_RGBA_DXT5 : (glob->type == kHapCodecSubType ? kHapCVPixelFormat_RGB_DXT1 : kHapCVPixelFormat_YCoCg_DXT5));
    
    if (isDXTPixelFormat(sourcePixelFormat))
    {
        size_t expectedDXTLength = dxtBytesForDimensions(glob->width, glob->height, glob->type);
        
        if (sourcePixelFormat != dxtPixelFormat // This shouldn't happen because we don't offer to convert between DXT formats
            || CVPixelBufferGetDataSize(task->sourceBuffer) < expectedDXTLength)
        {
            err = internalComponentErr;
            goto bail;
        }
        
        codec_src = sourceBaseAddress;
        codec_src_length = expectedDXTLength;
    }
    else
    {
        const void *encode_src;
        unsigned int encode_src_bytes_per_row;
        int result = 0;
        
        OSType wantedPixelFormat = glob->dxtEncoder->pixelformat_function(glob->dxtEncoder, sourcePixelFormat);
        
        // If necessary, convert the pixels to a format the encoder can ingest
        if (wantedPixelFormat != sourcePixelFormat)
        {
            formatConvertBuffer = HapCodecBufferCreate(glob->formatConvertPool);
            
            if (formatConvertBuffer == NULL)
            {
                err = memFullErr;
                goto bail;
            }
            
            switch (wantedPixelFormat)
            {
                case kHapCVPixelFormat_CoCgXY:
                    if (sourcePixelFormat == k32BGRAPixelFormat)
                    {
                        ConvertBGR_ToCoCg_Y8888((uint8_t *)sourceBaseAddress,
                                                (uint8_t *)HapCodecBufferGetBaseAddress(formatConvertBuffer),
                                                glob->width,
                                                glob->height,
                                                sourceBytesPerRow,
                                                glob->formatConvertBufferBytesPerRow);
                    }
                    else
                    {
                        ConvertRGB_ToCoCg_Y8888((uint8_t *)sourceBaseAddress,
                                                (uint8_t *)HapCodecBufferGetBaseAddress(formatConvertBuffer),
                                                glob->width,
                                                glob->height,
                                                sourceBytesPerRow,
                                                glob->formatConvertBufferBytesPerRow);
                    }
                    break;
                case k32RGBAPixelFormat:
                    if (sourcePixelFormat == k32BGRAPixelFormat)
                    {
                        uint8_t permuteMap[] = {2, 1, 0, 3};
                        ImageMath_Permute8888(sourceBaseAddress,
                                              sourceBytesPerRow,
                                              HapCodecBufferGetBaseAddress(formatConvertBuffer),
                                              glob->formatConvertBufferBytesPerRow,
                                              glob->width,
                                              glob->height,
                                              permuteMap);
                    }
                    else
                    {
                        err = internalComponentErr;
                        goto bail;
                    }
                    break;
                default:
                    err = internalComponentErr;
                    goto bail;
                    break;
            }
            
            encode_src = HapCodecBufferGetBaseAddress(formatConvertBuffer);
            encode_src_bytes_per_row = glob->formatConvertBufferBytesPerRow;
        }
        else // wantedPixelFormat == sourcePixelFormat
        {
            encode_src = sourceBaseAddress;
            encode_src_bytes_per_row = sourceBytesPerRow;
        }
        
        // Encode the DXT frame
        
        dxtBuffer = HapCodecBufferCreate(glob->dxtBufferPool);
        if (dxtBuffer == NULL)
        {
            err = memFullErr;
            goto bail;
        }
        
        result = glob->dxtEncoder->encode_function(glob->dxtEncoder,
                                                       encode_src,
                                                       encode_src_bytes_per_row,
                                                       wantedPixelFormat,
                                                       HapCodecBufferGetBaseAddress(dxtBuffer),
                                                       glob->width,
                                                       glob->height);
        
        if (result != 0)
        {
            err = internalComponentErr;
            goto bail;
        }
        
        // Unlock the source buffer as soon as we are finished with it
        // It has to be before we emit the frame because we lose our reference to it then
        CVPixelBufferUnlockBaseAddress(task->sourceBuffer, kHapCodecCVPixelBufferLockFlags);
        task->sourceBuffer = NULL;
        
#if defined(__APPLE__)
        // Detach the pixel buffer so it can be recycled (not on Windows as QT isn't thread-safe there)
        ICMCompressorSourceFrameDetachPixelBuffer(task->sourceFrame);
#endif
        
        // Return the format conversion buffer as soon as we can to minimise the number created
        HapCodecBufferReturn(formatConvertBuffer);
        formatConvertBuffer = NULL;
        
        codec_src = HapCodecBufferGetBaseAddress(dxtBuffer);
        codec_src_length = HapCodecBufferGetSize(dxtBuffer);
    }
    
    hapResult = HapEncode(codec_src,
                                       codec_src_length,
                                       glob->dxtFormat,
                                       HapCompressorSnappy,
                                       task->encodedFrameBaseAddress,
                                       glob->maxEncodedDataSize,
                                       &(task->encodedFrameActualSize));
    if (hapResult != HapResult_No_Error)
    {
        err = internalComponentErr;
        goto bail;
    }
    
#ifdef DEBUG
    HapCodecLockLock(&glob->lock);
    glob->debugFrameCount++;
    glob->debugLastFrameTime = CVGetCurrentHostTime();
    glob->debugTotalFrameBytes += task->encodedFrameActualSize;
    if (task->encodedFrameActualSize > glob->debugLargestFrameBytes) glob->debugLargestFrameBytes = task->encodedFrameActualSize;
    if (task->encodedFrameActualSize < glob->debugSmallestFrameBytes || glob->debugSmallestFrameBytes == 0) glob->debugSmallestFrameBytes = task->encodedFrameActualSize;
    HapCodecLockUnlock(&glob->lock);
#endif
bail:
    debug_print_err(glob, err);
    if (task->sourceBuffer) CVPixelBufferUnlockBaseAddress(task->sourceBuffer, kHapCodecCVPixelBufferLockFlags);
    HapCodecBufferReturn(formatConvertBuffer);
    HapCodecBufferReturn(dxtBuffer);
    
    task->error = err;
    
    // Queue the encoded frame for output
    queueEncodedFrame(glob, (HapCodecBufferRef)info);
}

// Presents the compressor with a frame to encode.
// The compressor may encode the frame immediately or queue it for later encoding.
// If the compressor queues the frame for later decode, it must retain it (by calling ICMCompressorSourceFrameRetain)
// and release it when it is done with it (by calling ICMCompressorSourceFrameRelease).
// Pixel buffers are guaranteed to conform to the pixel buffer options returned by ImageCodecPrepareToCompressFrames.
ComponentResult 
Hap_CEncodeFrame(
  HapCompressorGlobals glob,
  ICMCompressorSourceFrameRef sourceFrame,
  UInt32 flags HAP_ATTR_UNUSED)
{
	ComponentResult err = noErr;
    
    CVPixelBufferRef sourcePixelBuffer = ICMCompressorSourceFrameGetPixelBuffer(sourceFrame);
    OSType sourceFormat = CVPixelBufferGetPixelFormatType(sourcePixelBuffer);
    HapCodecBufferRef buffer;
    HapCodecCompressTask *task;
    ICMMutableEncodedFrameRef encodedFrame = NULL;
    
    if (!isDXTPixelFormat(sourceFormat))
    {
        // Create a DXT encoder if one will be needed by this frame
        if (glob->dxtEncoder == NULL)
        {
            if (glob->type == kHapYCoCgCodecSubType)
            {
                glob->dxtEncoder = HapCodecYCoCgDXTEncoderCreate();
            }
#if defined(__APPLE__)
            else if (glob->quality < codecHighQuality)
            {
                glob->dxtEncoder = HapCodecGLEncoderCreate(glob->width,
                                                           glob->height,
                                                           (glob->type == kHapAlphaCodecSubType ? kHapCVPixelFormat_RGBA_DXT5 : kHapCVPixelFormat_RGB_DXT1));
            }
#endif
            else
            {
                HapCodecSquishEncoderQuality squishQuality = (glob->quality < codecHighQuality ? HapCodecSquishEncoderWorstQuality : HapCodecSquishEncoderMediumQuality);
                OSType squishPixelFormat = (glob->type == kHapAlphaCodecSubType ? kHapCVPixelFormat_RGBA_DXT5 : kHapCVPixelFormat_RGB_DXT1);
                glob->dxtEncoder = HapCodecSquishEncoderCreate(squishQuality, squishPixelFormat);
            }
            
            if (glob->dxtEncoder == NULL)
            {
                err = internalComponentErr;
                goto bail;
            }
        }
        
        // Create a DXT buffer pool if one doesn't already exist
        if (glob->dxtBufferPool == NULL)
        {
            long wantedDXTSize = dxtBytesForDimensions(glob->width, glob->height, glob->type);
            
            glob->dxtBufferPool = HapCodecBufferPoolCreate(wantedDXTSize);
            
            if (glob->dxtBufferPool == NULL)
            {
                err = internalComponentErr;
                goto bail;
            }
        }
        
        if (glob->formatConvertPool == NULL)
        {
            OSType wantedPixelFormat = glob->dxtEncoder->pixelformat_function(glob->dxtEncoder, sourceFormat);
            
            // Create a buffer to convert the RGBA pixel buffer to an ordering the DXT encoder supports
            if (wantedPixelFormat != sourceFormat)
            {
                size_t wantedConvertBufferBytesPerRow = roundUpToMultipleOf16(glob->width * 4);
                size_t wantedBufferSize = wantedConvertBufferBytesPerRow * glob->height;
                
                glob->formatConvertPool = HapCodecBufferPoolCreate(wantedBufferSize);
                
                if (glob->formatConvertPool == NULL)
                {
                    err = internalComponentErr;
                    goto bail;
                }
                glob->formatConvertBufferBytesPerRow = wantedConvertBufferBytesPerRow;
            }

        }
    }
    
    // Create an empty encoded frame
    err = ICMEncodedFrameCreateMutable(glob->session, sourceFrame, glob->maxEncodedDataSize, &encodedFrame);
    if (err)
        goto bail;
    
    // Create a task and dispatch it
    buffer = HapCodecBufferCreate(glob->compressTaskPool);
    task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress(buffer);
    if (task == NULL)
    {
        err = memFullErr;
        goto bail;
    }
    
    task->sourceFrame = ICMCompressorSourceFrameRetain(sourceFrame);
    task->sourceBuffer = ICMCompressorSourceFrameGetPixelBuffer(sourceFrame);
    task->glob = glob;
    task->error = noErr;
    task->encodedFrame = encodedFrame;
    task->encodedFrameBaseAddress = ICMEncodedFrameGetDataPtr(encodedFrame);
    task->next = NULL;
    
    encodedFrame = NULL;
    
    HapCodecTasksAddTask(glob->taskGroup, buffer);
    
    // Dequeue and deliver any encoded frames
    do
    {
        buffer = dequeueNextFrameOut(glob);
        err = finishFrame(buffer);
        HapCodecBufferReturn(buffer);
        if (err != noErr)
        {
            goto bail;
        }
    } while (buffer != NULL);
    
bail:
    if (encodedFrame != NULL)
    {
        ICMEncodedFrameRelease(encodedFrame);
    }
    debug_print_err(glob, err);
	return err;
}

// Directs the compressor to finish with a queued source frame, either emitting or dropping it.
// This frame does not necessarily need to be the first or only source frame emitted or dropped
// during this call, but the compressor must call either ICMCompressorSessionDropFrame or 
// ICMCompressorSessionEmitEncodedFrame with this frame before returning.
// The ICM will call this function to force frames to be encoded for the following reasons:
//   - the maximum frame delay count or maximum frame delay time in the sessionOptions
//     does not permit more frames to be queued
//   - the client has called ICMCompressionSessionCompleteFrames.
ComponentResult 
Hap_CCompleteFrame( 
  HapCompressorGlobals glob,
  ICMCompressorSourceFrameRef sourceFrame HAP_ATTR_UNUSED,
  UInt32 flags HAP_ATTR_UNUSED)
{
    ComponentResult err = noErr;
    HapCodecBufferRef buffer;
    
    if (glob->taskGroup)
    {
        // This waits for all pending frames rather than the particular frame
        // It makes the progress-bar jerky in QuickTime Player but otherwise isn't
        // a problem.
        HapCodecTasksWaitForGroupToComplete(glob->taskGroup);
    }
    
    do
    {
        buffer = dequeueNextFrameOut(glob);
        
        err = finishFrame(buffer);
        HapCodecBufferReturn(buffer);
        if (err != noErr)
        {
            goto bail;
        }

    } while (buffer != NULL);
    
bail:
	return err;
}

static void releaseTaskFrames(HapCodecCompressTask *task)
{
    if (task)
    {
        ICMCompressorSourceFrameRelease(task->sourceFrame);
        ICMEncodedFrameRelease(task->encodedFrame);
    }
}

static ComponentResult finishFrame(HapCodecBufferRef buffer)
{
    ComponentResult err = noErr;
    if (buffer != NULL)
    {
        HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress(buffer);
        
        err = task->error;
        
        task->glob->lastFrameOut = ICMCompressorSourceFrameGetDisplayNumber(task->sourceFrame);
        
        if (err == noErr)
        {
            err = ICMEncodedFrameSetDataSize(task->encodedFrame, task->encodedFrameActualSize);
        }
        if (err == noErr)
        {
            err = ICMEncodedFrameSetMediaSampleFlags(task->encodedFrame, mediaSampleDroppable | mediaSampleDoesNotDependOnOthers);
        }
        if (err == noErr)
        {
            err = ICMEncodedFrameSetFrameType( task->encodedFrame, kICMFrameType_I );
        }
        if (err == noErr)
        {
            ICMCompressorSessionEmitEncodedFrame(task->glob->session, task->encodedFrame, 1, &task->sourceFrame);
        }
        else
        {
            ICMCompressorSessionDropFrame(task->glob->session, task->sourceFrame);
        }
        releaseTaskFrames(task);
    }
    return err;
}

static void queueEncodedFrame(HapCompressorGlobals glob, HapCodecBufferRef frame)
{
    HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress(frame);
    if (glob && task)
    {
        HapCodecLockLock(&glob->lock);
        task->next = glob->finishedFrames;
        glob->finishedFrames = frame;
        HapCodecLockUnlock(&glob->lock);
    }
}

static HapCodecBufferRef dequeueNextFrameOut(HapCompressorGlobals glob)
{
    HapCodecBufferRef found = NULL;
    if (glob)
    {
        HapCodecBufferRef *prevPtr;
        HapCodecBufferRef buffer;
        
        HapCodecLockLock(&glob->lock);
        prevPtr = &glob->finishedFrames;
        buffer = glob->finishedFrames;
        while (buffer) {
            HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress(buffer);
            if (task)
            {
                long frameNumber = ICMCompressorSourceFrameGetDisplayNumber(task->sourceFrame);
                if (frameNumber == glob->lastFrameOut + 1)
                {
                    found = buffer;
                    *prevPtr = task->next;
                    break;
                }
                buffer = task->next;
                prevPtr = &task->next;
            }
            else
            {
                buffer = NULL;
            }
            
        }
        HapCodecLockUnlock(&glob->lock);
    }
    return found;
}
