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
#include "ParallelLoops.h"
#include "Buffers.h"
#include "DXTEncoder.h"
#include "ImageMath.h"
#if defined(DEBUG)
#include <string.h>
#endif
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
    
    uint8_t                         *formatConvertBuffer;
    size_t                          formatConvertBufferBytesPerRow;
    
    HapCodecBufferPoolRef           dxtBufferPool;
    
    unsigned int                    dxtFormat;
    HapCodecTaskGroupRef            taskGroup;

    unsigned int                    sliceCount;
#ifdef DEBUG
    unsigned int                    debugFrameCount;
    uint64_t                        debugStartTime;
    uint64_t                        debugLastFrameTime;
    unsigned long                   debugTotalFrameBytes;
    unsigned long                   debugLargestFrameBytes;
    unsigned long                   debugSmallestFrameBytes;    
#endif
} HapCompressorGlobalsRecord, *HapCompressorGlobals;

typedef struct HapCodecEncodeDXTTask HapCodecEncodeDXTTask;

struct HapCodecEncodeDXTTask
{
    long                    width;
    long                    height;
    long                    sliceHeight;
    OSType                  sourcePixelFormat;
    size_t                  sourceBytesPerRow;
    const uint8_t           *source;
    OSType                  dxtInputFormat;
    size_t                  dxtInputBytesPerRow;
    uint8_t                 *dxtInput;
    size_t                  dxtBytesPerRow;
    uint8_t                 *dxt;
    HapCodecDXTEncoderRef   encoder;
};

struct HapCodecCompressTask
{
    HapCompressorGlobals            glob;
    ICMCompressorSourceFrameRef     sourceFrame;
    CVPixelBufferRef                sourceFramePixelBuffer;
    const uint8_t                   *sourceFramePixelBufferBaseAddress;
    ICMMutableEncodedFrameRef       encodedFrame;
    UInt8                           *encodedFrameDataPtr;
    unsigned long                   encodedFrameActualSize;
    HapCodecBufferRef               dxtBuffer;
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
/*
 createTask() returns a complete task or NULL on error (which is likely to be due to buffer allocation failure)
 */
static HapCodecBufferRef createTask(HapCompressorGlobals glob, HapCodecBufferRef dxtBuffer, ICMCompressorSourceFrameRef sourceFrame, ICMMutableEncodedFrameRef encodedFrame);
static void disposeTask(HapCodecCompressTask *task);
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
    glob->formatConvertBuffer = NULL;
    glob->formatConvertBufferBytesPerRow = 0;
    glob->dxtBufferPool = NULL;
    glob->dxtFormat = 0;
    glob->taskGroup = NULL;
    glob->sliceCount = 1;
    
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
        
        free(glob->formatConvertBuffer);
        glob->formatConvertBuffer = NULL;
        
        HapCodecTasksWaitForGroupToComplete(glob->taskGroup);
        HapCodecTasksDestroyGroup(glob->taskGroup);
        glob->taskGroup = NULL;
        
        // We should never have queued frames when we are closed
        // but we check and properly release the memory if we do
        
        buffer = glob->finishedFrames;
        while (buffer) {
            HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress(buffer);
            HapCodecBufferRef next;
            if (task)
            {
                disposeTask(task);
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
	HapCompressorGlobals glob,
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

// A worst-case estimate of bytes per frame
static size_t estimateBytesPerFrame(HapCompressorGlobals glob)
{
    size_t dxtSize = dxtBytesForDimensions(glob->width, glob->height, glob->type);
    size_t outputSize = glob->maxEncodedDataSize;
    size_t encodeSize = sizeof(HapCodecCompressTask);
    return dxtSize + outputSize + encodeSize;
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
    int maxTasks;

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

    // Only use half of addressable memory (we're 32-bit, so UINT32_MAX)
    maxTasks = (UINT32_MAX / 2) / estimateBytesPerFrame(glob);
    // hapCodecMaxTasks() returns any system or host restrictions
    if (maxTasks > hapCodecMaxTasks())
        maxTasks = hapCodecMaxTasks();
    
    glob->taskGroup = HapCodecTasksCreateGroup(Background_Encode, maxTasks);

    glob->sliceCount = roundUpToMultipleOf4(glob->height) / 4;
    if (glob->sliceCount > 32)
        glob->sliceCount = 32;
    
    // decrease slice count until it yeilds whole DXT rows
    while ((roundUpToMultipleOf4(glob->height) / 4) % glob->sliceCount != 0)
        glob->sliceCount--;

#ifdef DEBUG    
    glob->debugStartTime = CVGetCurrentHostTime();
#endif
bail:
	if( compressorPixelBufferAttributes ) CFRelease( compressorPixelBufferAttributes );
    debug_print_err(glob, err);
	return err;
}

static void Encode_Slice(void *p, unsigned int index)
{
    HapCodecEncodeDXTTask *task = (HapCodecEncodeDXTTask *)p;

    unsigned int sliceHeight = task->sliceHeight;
    if ((index + 1) * sliceHeight > (unsigned int)task->height)
        sliceHeight = task->height - (index * sliceHeight);

    const uint8_t *src = task->source + (index * task->sliceHeight * task->sourceBytesPerRow);
    uint8_t *dxtInput = task->dxtInput + (index * task->sliceHeight * task->dxtInputBytesPerRow);
    uint8_t *dxt = task->dxt + (index * task->sliceHeight * task->dxtBytesPerRow);

    if (task->dxtInputFormat != task->sourcePixelFormat)
    {
        // for all these we pass 0 as last argument so we don't tile, we are already multithreaded
        switch (task->dxtInputFormat)
        {
            case kHapCVPixelFormat_CoCgXY:
                if (task->sourcePixelFormat == k32BGRAPixelFormat)
                {
                    ConvertBGR_ToCoCg_Y8888(src,
                                            dxtInput,
                                            task->width,
                                            sliceHeight,
                                            task->sourceBytesPerRow,
                                            task->dxtInputBytesPerRow,
                                            0);
                }
                else
                {
                    ConvertRGB_ToCoCg_Y8888(src,
                                            dxtInput,
                                            task->width,
                                            sliceHeight,
                                            task->sourceBytesPerRow,
                                            task->dxtInputBytesPerRow,
                                            0);
                }
                break;
            case k32RGBAPixelFormat:
                if (task->sourcePixelFormat == k32BGRAPixelFormat)
                {
                    uint8_t permuteMap[] = {2, 1, 0, 3};
                    ImageMath_Permute8888(src,
                                          task->sourceBytesPerRow,
                                          dxtInput,
                                          task->dxtInputBytesPerRow,
                                          task->width,
                                          sliceHeight,
                                          permuteMap,
                                          0);
                }
                break;
            default:
                break;
        }
    }

    if (task->encoder->can_slice)
    {
        // Encode the DXT frame
        task->encoder->encode_function(task->encoder,
                                       dxtInput,
                                       task->dxtInputBytesPerRow,
                                       task->dxtInputFormat,
                                       dxt,
                                       task->width,
                                       sliceHeight);
    }
}

static void Background_Encode(void *info)
{
    HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress((HapCodecBufferRef)info);
    HapCompressorGlobals glob = task->glob;

    const void *codec_src = NULL;
    unsigned int codec_src_length = 0;

    unsigned int hapResult;
    
    ComponentResult err = noErr;
    
    if (task->dxtBuffer)
    {
        codec_src = HapCodecBufferGetBaseAddress(task->dxtBuffer);
        codec_src_length = HapCodecBufferGetSize(task->dxtBuffer);
    }
    else
    {
        codec_src = task->sourceFramePixelBufferBaseAddress;
        codec_src_length = dxtBytesForDimensions(glob->width, glob->height, glob->type);
    }

    hapResult = HapEncode(codec_src,
                          codec_src_length,
                          glob->dxtFormat,
                          HapCompressorSnappy,
                          task->encodedFrameDataPtr,
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

    if (task->dxtBuffer)
    {
        HapCodecBufferReturn(task->dxtBuffer);
        task->dxtBuffer = NULL;
    }
#if defined(__APPLE__)
    if (task->sourceFramePixelBuffer)
    {
        CVPixelBufferUnlockBaseAddress(task->sourceFramePixelBuffer, kHapCodecCVPixelBufferLockFlags);
        // Detach the pixel buffer so it can be recycled (not on Windows as QT isn't thread-safe there)
        ICMCompressorSourceFrameDetachPixelBuffer(task->sourceFrame);
        task->sourceFramePixelBuffer = NULL;
    }
#endif

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
    HapCodecBufferRef buffer = NULL;
    HapCodecBufferRef dxtBuffer = NULL;
    ICMMutableEncodedFrameRef encodedFrame = NULL;

    if (CVPixelBufferGetWidth(sourcePixelBuffer) != glob->width || CVPixelBufferGetHeight(sourcePixelBuffer) != glob->height)
        return internalComponentErr;

    if (isDXTPixelFormat(sourceFormat))
    {
        size_t expectedDXTLength = dxtBytesForDimensions(glob->width, glob->height, glob->type);

        if (CVPixelBufferGetDataSize(sourcePixelBuffer) < expectedDXTLength)
            return internalComponentErr;
    }
    else
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
        
        if (glob->formatConvertBuffer == NULL)
        {
            OSType wantedPixelFormat = glob->dxtEncoder->pixelformat_function(glob->dxtEncoder, sourceFormat);
            
            // Create a buffer to convert the RGBA pixel buffer to an ordering the DXT encoder supports
            if (wantedPixelFormat != sourceFormat)
            {
                size_t wantedConvertBufferBytesPerRow = roundUpToMultipleOf16(glob->width * 4);
                size_t wantedBufferSize = wantedConvertBufferBytesPerRow * glob->height;
#if defined(__APPLE__)
                glob->formatConvertBuffer = malloc(wantedBufferSize);
#else
                glob->formatConvertBuffer = (uint8_t *)_aligned_malloc(wantedBufferSize, 16);
#endif

                if (glob->formatConvertBuffer == NULL)
                {
                    err = memFullErr;
                    goto bail;
                }
                glob->formatConvertBufferBytesPerRow = wantedConvertBufferBytesPerRow;
            }
        }
    }

    if (!isDXTPixelFormat(sourceFormat))
    {
        // Perform DXT compression
        HapCodecEncodeDXTTask dxtTask;

        if (CVPixelBufferLockBaseAddress(sourcePixelBuffer, kHapCodecCVPixelBufferLockFlags) != kCVReturnSuccess)
        {
            err = internalComponentErr;
            goto bail;
        }

        dxtBuffer = HapCodecBufferCreate(glob->dxtBufferPool);

        if (dxtBuffer == NULL)
        {
            // Try again after finishing any background frames
            Hap_CCompleteFrame(glob, NULL, 0);
            dxtBuffer = HapCodecBufferCreate(glob->dxtBufferPool);
        }

        if (dxtBuffer == NULL)
        {
            err = memFullErr;
            goto bail;
        }

        dxtTask.width = glob->width;
        dxtTask.height = glob->height;
        dxtTask.encoder = glob->dxtEncoder;
        dxtTask.sliceHeight = roundUpToMultipleOf4(glob->height) / glob->sliceCount;
        dxtTask.sourceBytesPerRow = CVPixelBufferGetBytesPerRow(sourcePixelBuffer);
        dxtTask.sourcePixelFormat = sourceFormat;
        dxtTask.source = CVPixelBufferGetBaseAddress(sourcePixelBuffer);
        dxtTask.dxtInputFormat = glob->dxtEncoder->pixelformat_function(glob->dxtEncoder, sourceFormat);
        dxtTask.dxtBytesPerRow = roundUpToMultipleOf4(glob->width);
        if (glob->type == kHapCodecSubType)
            dxtTask.dxtBytesPerRow /= 2;

        // If necessary, convert the pixels to a format the encoder can ingest
        if (dxtTask.dxtInputFormat != sourceFormat)
        {
            if ((dxtTask.dxtInputFormat == k32RGBAPixelFormat && dxtTask.dxtInputFormat != k32BGRAPixelFormat) ||
                (dxtTask.dxtInputFormat != kHapCVPixelFormat_CoCgXY && dxtTask.dxtInputFormat != k32RGBAPixelFormat))
            {
                err = internalComponentErr;
                goto bail;
            }
            dxtTask.dxtInput = glob->formatConvertBuffer;
            dxtTask.dxtInputBytesPerRow = glob->formatConvertBufferBytesPerRow;
        }
        else // wantedPixelFormat == sourcePixelFormat
        {
            dxtTask.dxtInput = (uint8_t *)dxtTask.source;
            dxtTask.dxtInputBytesPerRow = dxtTask.sourceBytesPerRow;
        }

        dxtTask.dxt = HapCodecBufferGetBaseAddress(dxtBuffer);

        HapParallelFor(Encode_Slice, &dxtTask, glob->sliceCount);

        if (dxtTask.encoder->can_slice == false)
        {
            dxtTask.encoder->encode_function(dxtTask.encoder,
                                             dxtTask.dxtInput,
                                             dxtTask.dxtInputBytesPerRow,
                                             dxtTask.dxtInputFormat,
                                             dxtTask.dxt,
                                             dxtTask.width,
                                             dxtTask.height);
        }

        CVPixelBufferUnlockBaseAddress(sourcePixelBuffer, kHapCodecCVPixelBufferLockFlags);

        // Detach the sourceFrame as soon as we are finished with it
        ICMCompressorSourceFrameDetachPixelBuffer(sourceFrame);
        sourcePixelBuffer = NULL;
    }

    // Create an empty encoded frame
    err = ICMEncodedFrameCreateMutable(glob->session, sourceFrame, glob->maxEncodedDataSize, &encodedFrame);
    if (err != noErr)
        encodedFrame = NULL;
    
    if (err == noErr)
    {
        buffer = createTask(glob, dxtBuffer, sourceFrame, encodedFrame);
        if (buffer == NULL)
            err = memFullErr;
    }

    if (err == memFullErr)
    {
        // in case of memory allocation failure, finish all pending tasks and try again
        Hap_CCompleteFrame(glob, NULL, 0);

        if (encodedFrame == NULL)
            err = ICMEncodedFrameCreateMutable(glob->session, sourceFrame, glob->maxEncodedDataSize, &encodedFrame);

        if (err == noErr)
            buffer = createTask(glob, dxtBuffer, sourceFrame, encodedFrame);
    }

    ICMEncodedFrameRelease(encodedFrame);
    encodedFrame = NULL;

    if (buffer == NULL)
    {
        err = memFullErr;
        goto bail;
    }

    HapCodecTasksAddTask(glob->taskGroup, buffer);
    sourceFrame = NULL; // indicate to bail: that we don't need to drop it
    dxtBuffer = NULL; // ditto

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
    if (sourceFrame)
        ICMCompressorSessionDropFrame(glob->session, sourceFrame);
    HapCodecBufferReturn(dxtBuffer);
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

static void disposeTask(HapCodecCompressTask *task)
{
    if (task)
    {
        if (task->sourceFramePixelBuffer)
        {
            CVPixelBufferUnlockBaseAddress(task->sourceFramePixelBuffer, kHapCodecCVPixelBufferLockFlags);
        }
        ICMCompressorSourceFrameRelease(task->sourceFrame);
        ICMEncodedFrameRelease(task->encodedFrame);
        HapCodecBufferReturn(task->dxtBuffer);
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
        disposeTask(task);
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

static HapCodecBufferRef createTask(HapCompressorGlobals glob, HapCodecBufferRef dxtBuffer, ICMCompressorSourceFrameRef sourceFrame, ICMMutableEncodedFrameRef encodedFrame)
{
    HapCodecBufferRef buffer = HapCodecBufferCreate(glob->compressTaskPool);
    if (buffer)
    {
        HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress(buffer);

        task->sourceFrame = ICMCompressorSourceFrameRetain(sourceFrame);
        task->sourceFramePixelBuffer = ICMCompressorSourceFrameGetPixelBuffer(sourceFrame);
        if (task->sourceFramePixelBuffer)
        {
            CVPixelBufferLockBaseAddress(task->sourceFramePixelBuffer, kHapCodecCVPixelBufferLockFlags);
            task->sourceFramePixelBufferBaseAddress = CVPixelBufferGetBaseAddress(task->sourceFramePixelBuffer);
        }
        else
        {
            task->sourceFramePixelBufferBaseAddress = NULL;
        }
        task->glob = glob;
        task->encodedFrame = (ICMMutableEncodedFrameRef)ICMEncodedFrameRetain(encodedFrame);
        task->encodedFrameDataPtr = ICMEncodedFrameGetDataPtr(encodedFrame);
        task->error = noErr;
        task->next = NULL;
        task->dxtBuffer = dxtBuffer;
    }
    return buffer;
}
