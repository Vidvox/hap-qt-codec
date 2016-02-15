/*
 HapDecompressor.c
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
#else
    #include <ConditionalMacros.h>
    #include <ImageCodec.h>
    #include <stdlib.h>
    #include <malloc.h>
#endif

#include "HapPlatform.h"
#include "HapCodecVersion.h"
#include "Utility.h"
#include "PixelFormats.h"
#include "HapCodecSubTypes.h"
#include "hap.h"
#include "Buffers.h"
#include "ParallelLoops.h"
#include "YCoCg.h"
#include "YCoCgDXT.h"

/*
 Defines determine the decoding methods available.
 MacOS supports the required EXT_texture_compression_s3tc extension at least as far back as 10.5
 so we don't enable squish decoding at all. For Windows we currently use Squish.
 If both are enabled, squish will be used as a fallback when the extension is not available.
 */

#if defined(__APPLE__)
#define HAP_GPU_DECODE
#else
#define HAP_SQUISH_DECODE
#endif

#ifndef HAP_GPU_DECODE
    #ifndef HAP_SQUISH_DECODE
        #error Neither HAP_GPU_DECODE nor HAP_SQUISH_DECODE is defined. #define one or both.
    #endif
#endif

#ifdef HAP_GPU_DECODE
    #include "HapCodecGL.h"
#endif

#ifdef HAP_SQUISH_DECODE
    #include "SquishDecoder.h"
#endif

#include "SquishRGTC1Decoder.h"

// Data structures
typedef struct	{
	ComponentInstance			self;
	ComponentInstance			delegateComponent;
	ComponentInstance			target;
    OSType                      type;
	long						width;
	long						height;
    long                        dxtWidth;
    long                        dxtHeight;
	Handle						wantedDestinationPixelTypes;
    HapCodecBufferPoolRef       dxtBufferPool;
    HapCodecBufferPoolRef       alphaBufferPool;
    HapCodecBufferPoolRef       convertBufferPool;
#ifdef HAP_GPU_DECODE
    HapCodecGLRef               glDecoder;
#endif
} HapDecompressorGlobalsRecord, *HapDecompressorGlobals;

typedef struct {
	long                        width;
	long                        height;
    long                        dxtWidth;
    long                        dxtHeight;
	size_t                      dataSize;
    Boolean                     hasColour;
    Boolean                     hasAlpha;
    unsigned int                dxtFormat;
    unsigned int                dxtIndex;
    unsigned int                alphaIndex;
    OSType                      destFormat;
	Boolean                     decoded;
    HapCodecBufferRef           dxtBuffer;
    HapCodecBufferRef           alphaBuffer;
    HapCodecBufferRef           convertBuffer; // used for YCoCg -> RGB
} HapDecompressRecord;

// Setup required for ComponentDispatchHelper.c
#define IMAGECODEC_BASENAME() 		Hap_D
#define IMAGECODEC_GLOBALS() 		HapDecompressorGlobals storage

#define CALLCOMPONENT_BASENAME()	IMAGECODEC_BASENAME()
#define	CALLCOMPONENT_GLOBALS()		IMAGECODEC_GLOBALS()

#define QT_BASENAME()				CALLCOMPONENT_BASENAME()
#define QT_GLOBALS()				CALLCOMPONENT_GLOBALS()

#define COMPONENT_UPP_PREFIX()		uppImageCodec
#define COMPONENT_DISPATCH_FILE		"HapDecompressorDispatch.h"
#define COMPONENT_SELECT_PREFIX()  	kImageCodec

#define	GET_DELEGATE_COMPONENT()	(storage->delegateComponent)

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

struct PlanarPixmapInfoHapYCoCgA {
    PlanarComponentInfo  componentInfoYCoCgDXT5;
    PlanarComponentInfo  componentInfoARGTC1;
};

typedef struct PlanarPixmapInfoHapYCoCgA PlanarPixmapInfoHapYCoCgA;
/*
 Callback for multithreaded Hap decoding
 */

void HapMTDecode(HapDecodeWorkFunction function, void *p, unsigned int count, void *info HAP_ATTR_UNUSED)
{
    HapParallelFor((HapParallelFunction)function, p, count);
}

/* -- This Image Decompressor User the Base Image Decompressor Component --
	The base image decompressor is an Apple-supplied component
	that makes it easier for developers to create new decompressors.
	The base image decompressor does most of the housekeeping and
	interface functions required for a QuickTime decompressor component,
	including scheduling for asynchronous decompression.
*/

// Component Open Request - Required
#pragma GCC push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
ComponentResult Hap_DOpen(HapDecompressorGlobals glob, ComponentInstance self)
{
    ComponentResult err;
    ComponentDescription componentDescription;

    // Enable the following line to attach a debugger when a background helper is launched (eg for QuickTime Player X)
//    raise(SIGSTOP);

	// Allocate memory for our globals, set them up and inform the component manager that we've done so
    
#ifndef __clang_analyzer__
	glob = (HapDecompressorGlobals)calloc( sizeof( HapDecompressorGlobalsRecord ), 1 );
#endif
	if( ! glob ) {
		err = memFullErr;
		goto bail;
	}

	SetComponentInstanceStorage(self, (Handle)glob);
    
    // Store the type of decompressor we are (Hap or Hap YCoCg)
    GetComponentInfo((Component)self, &componentDescription, 0, 0, 0);
    
	glob->self = self;
	glob->target = self;
    glob->type = componentDescription.componentSubType;
	glob->dxtBufferPool = NULL;
    glob->alphaBufferPool = NULL;
    glob->convertBufferPool = NULL;

#ifdef HAP_GPU_DECODE
    glob->glDecoder = NULL;
#endif
    
	// Open and target an instance of the base decompressor as we delegate
	// most of our calls to the base decompressor instance
	err = OpenADefaultComponent(decompressorComponentType, kBaseCodecType, &glob->delegateComponent);
	if( err )
		goto bail;
    

	ComponentSetTarget(glob->delegateComponent, self);


bail:
    debug_print_err(glob, err);
	return err;
}
#pragma GCC pop

// Component Close Request - Required
ComponentResult Hap_DClose(HapDecompressorGlobals glob, ComponentInstance self HAP_ATTR_UNUSED)
{
	// Make sure to close the base component and deallocate our storage
	if (glob)
    {
		if (glob->delegateComponent) {
			CloseComponent(glob->delegateComponent);
		}
		
        HapCodecBufferPoolDestroy(glob->dxtBufferPool);
        HapCodecBufferPoolDestroy(glob->alphaBufferPool);
#ifdef HAP_GPU_DECODE
        if (glob->glDecoder)
        {
            HapCodecGLDestroy(glob->glDecoder);
        }
#endif
        HapCodecBufferPoolDestroy(glob->convertBufferPool);
        
		DisposeHandle( glob->wantedDestinationPixelTypes );
		glob->wantedDestinationPixelTypes = NULL;
		
		free( glob );
	}

	return noErr;
}

// Component Version Request - Required
ComponentResult Hap_DVersion(HapDecompressorGlobals glob HAP_ATTR_UNUSED)
{
	return kHapDecompressorVersion;
}

// Component Target Request
// 		Allows another component to "target" you i.e., you call another component whenever
// you would call yourself (as a result of your component being used by another component)
ComponentResult Hap_DTarget(HapDecompressorGlobals glob, ComponentInstance target)
{
	glob->target = target;
	return noErr;
}

#pragma mark-

// ImageCodecInitialize
//		The first function call that your image decompressor component receives from the base image
// decompressor is always a call to ImageCodecInitialize. In response to this call, your image decompressor
// component returns an ImageSubCodecDecompressCapabilities structure that specifies its capabilities.
ComponentResult Hap_DInitialize(HapDecompressorGlobals glob HAP_ATTR_UNUSED, ImageSubCodecDecompressCapabilities *cap)
{
	// Secifies the size of the ImageSubCodecDecompressRecord structure
	// and say we can support asyncronous decompression
	// With the help of the base image decompressor, any image decompressor
	// that uses only interrupt-safe calls for decompression operations can
	// support asynchronous decompression.
	cap->decompressRecordSize = sizeof(HapDecompressRecord);
	cap->canAsync = true;

	// These fields were added in QuickTime 7.  Be safe.
	if( cap->recordSize > offsetof(ImageSubCodecDecompressCapabilities, baseCodecShouldCallDecodeBandForAllFrames) ) {
		// Tell the base codec that we are "multi-buffer aware".
		// This promises that we always draw using the ImageSubCodecDecompressRecord.baseAddr/rowBytes 
		// passed to our ImageCodecDrawBand function, and that we always overwrite every pixel in the buffer.
		// It is important to set this in order to get optimal performance when playing through CoreVideo.
		cap->subCodecIsMultiBufferAware = true;
		
		// Ask the base codec to call our ImageCodecDecodeBand function for every frame.
		// If you do not set this, then your ImageCodecDrawBand function must
		// manually ensure that the frame is decoded before drawing it.
		cap->baseCodecShouldCallDecodeBandForAllFrames = true;
	}

	return noErr;
}

// ImageCodecPreflight
// 		The base image decompressor gets additional information about the capabilities of your image
// decompressor component by calling ImageCodecPreflight. The base image decompressor uses this
// information when responding to a call to the ImageCodecPredecompress function,
// which the ICM makes before decompressing an image. You are required only to provide values for
// the wantedDestinationPixelSize and wantedDestinationPixelTypes fields and can also modify other
// fields if necessary.
ComponentResult Hap_DPreflight(HapDecompressorGlobals glob, CodecDecompressParams *p)
{
    // see also p->destinationBufferMemoryPreference and kICMImageBufferPreferVideoMemory
	OSStatus err = noErr;
	CodecCapabilities *capabilities = p->capabilities;
	int widthRoundedUp, heightRoundedUp;
    
	// Specify the minimum image band height supported by the component
	// bandInc specifies a common factor of supported image band heights - 
	// if your component supports only image bands that are an even
    // multiple of some number of pixels high report this common factor in bandInc
	capabilities->bandMin = (**p->imageDescription).height; // Only handle full frame-height (don't split into smaller parts)
	capabilities->bandInc = capabilities->bandMin;

    /*
     http://lists.apple.com/archives/quicktime-api/2008/Sep/msg00130.html

     In your codec's Preflight function, for some applications you also need to set the 'preferredOffscreenPixelSize' parameter
     */
    
	// Indicate the pixel depth the component can use with the specified image
	capabilities->wantedPixelSize = 0; // set this to zero when using wantedDestinationPixelTypes
	if( NULL == glob->wantedDestinationPixelTypes )
    {
		glob->wantedDestinationPixelTypes = NewHandleClear( 4 * sizeof(OSType) );
		if( NULL == glob->wantedDestinationPixelTypes )
        {
            err =  memFullErr;
            goto bail;
        }
	}
	p->wantedDestinationPixelTypes = (OSType **)glob->wantedDestinationPixelTypes;
    
    // List the QT-native formats first, otherwise QT will prefer our custom formats when transcoding,
    // but not be able to do anything with them
    (*p->wantedDestinationPixelTypes)[0] = k32RGBAPixelFormat;
	(*p->wantedDestinationPixelTypes)[1] = k32BGRAPixelFormat;
    
    switch (glob->type) {
        case kHapCodecSubType:
            (*p->wantedDestinationPixelTypes)[2] = kHapCVPixelFormat_RGB_DXT1;
            break;
        case kHapYCoCgCodecSubType:
            (*p->wantedDestinationPixelTypes)[2] = kHapCVPixelFormat_YCoCg_DXT5;
            break;
        case kHapAlphaCodecSubType:
            (*p->wantedDestinationPixelTypes)[2] = kHapCVPixelFormat_RGBA_DXT5;
            break;
        case kHapYCoCgACodecSubType:
            (*p->wantedDestinationPixelTypes)[2] = kHapCVPixelFormat_YCoCg_DXT5_A_RGTC1;
            break;
        default:
            err = internalComponentErr;
            goto bail;
            break;
    }

	(*p->wantedDestinationPixelTypes)[3] = 0;
    
	// Specify the number of pixels the image must be extended in width and height if
	// the component cannot accommodate the image at its given width and height.
	// This codec must have output buffers that are rounded up to multiples of 4x4.
    
	glob->width = (**p->imageDescription).width;
	glob->height = (**p->imageDescription).height;
    
    widthRoundedUp = roundUpToMultipleOf4(glob->width);
    heightRoundedUp = roundUpToMultipleOf4(glob->height);    

    // Although ExampleIPBCodec creates extended pixels as the difference between the dimensions in the image description
    // and the desired buffer dimensions, code using a home-made GWorld gets stuck because it appears to interpret the
    // extended dimensions as relative to the GWorld, not the image description
    if (p->dstPixMap.bounds.right == widthRoundedUp)
        capabilities->extendWidth = 0;
    else
        capabilities->extendWidth = (short)(widthRoundedUp - glob->width);

    if (p->dstPixMap.bounds.bottom == heightRoundedUp)
        capabilities->extendHeight = 0;
    else
        capabilities->extendHeight = (short)(heightRoundedUp - glob->height);

    glob->dxtWidth = widthRoundedUp;
    glob->dxtHeight = heightRoundedUp;
    
bail:
    debug_print_err(glob, err);
	return err;
}

// ImageCodecBeginBand
// 		The ImageCodecBeginBand function allows your image decompressor component to save information about
// a band before decompressing it. This function is never called at interrupt time. The base image decompressor
// preserves any changes your component makes to any of the fields in the ImageSubCodecDecompressRecord
// or CodecDecompressParams structures. If your component supports asynchronous scheduled decompression, it
// may receive more than one ImageCodecBeginBand call before receiving an ImageCodecDrawBand call.
ComponentResult Hap_DBeginBand(HapDecompressorGlobals glob, CodecDecompressParams *p, ImageSubCodecDecompressRecord *drp, long flags HAP_ATTR_UNUSED)
{
	OSStatus err = noErr;
	HapDecompressRecord *myDrp = (HapDecompressRecord *)drp->userDecompressRecord;
    unsigned int hap_result;
    unsigned int frame_texture_count;

	myDrp->width = (**p->imageDescription).width;
	myDrp->height = (**p->imageDescription).height;
    myDrp->dxtWidth = glob->dxtWidth;
    myDrp->dxtHeight = glob->dxtHeight;
    myDrp->convertBuffer = myDrp->dxtBuffer = myDrp->alphaBuffer = NULL;
    myDrp->alphaIndex = myDrp->dxtIndex;
    myDrp->hasAlpha = myDrp->hasColour = false;
    
    if (myDrp->width != glob->width || myDrp->height != glob->height)
    {
        err = internalComponentErr;
        goto bail;
    }
	// Unfortunately, the image decompressor API can not quite guarantee to tell the decompressor
	// how much data is available, because the deprecated API DecompressSequenceFrame does not take 
	// a dataSize argument.  (That's why you should call DecompressSequenceFrameS instead.)
	// Here's the best effort we can make: if there's a data-loading proc, use the dataSize from the 
	// image description; otherwise, use the bufferSize.
	if( drp->dataProcRecord.dataProc )
		myDrp->dataSize = (**p->imageDescription).dataSize;
	else
		myDrp->dataSize = p->bufferSize;
	
	// In some cases, a frame will be decoded and ready for display, but the display will be cancelled.  
	// QuickTime's video media handler will remember that the frame has already been decoded,
	// and if appropriate, will schedule that frame for display without redecoding by using the 
	// icmFrameAlreadyDecoded flag.  
	// In that case, we should simply retrieve the frame from whichever buffer we put it in.
    
	myDrp->decoded = p->frameTime ? (0 != (p->frameTime->flags & icmFrameAlreadyDecoded)) : false;
    
    myDrp->destFormat = p->dstPixMap.pixelFormat;
    
    // Inspect the frame header to discover the texture format(s)
    hap_result = HapGetFrameTextureCount(drp->codecData, myDrp->dataSize, &frame_texture_count);
    if (hap_result != HapResult_No_Error)
    {
        err = internalComponentErr;
        goto bail;
    }

    for (unsigned int i = 0; i < frame_texture_count; i++) {
        unsigned int texture_format;
        hap_result = HapGetFrameTextureFormat(drp->codecData, myDrp->dataSize, i, &texture_format);
        if (hap_result != HapResult_No_Error)
        {
            err = internalComponentErr;
            goto bail;
        }
        if (texture_format == HapTextureFormat_A_RGTC1)
        {
            myDrp->alphaIndex = i;
            myDrp->hasAlpha = true;
        }
        else if (texture_format == HapTextureFormat_RGB_DXT1 || texture_format == HapTextureFormat_RGBA_DXT5 || texture_format == HapTextureFormat_YCoCg_DXT5)
        {
            myDrp->dxtIndex = i;
            myDrp->dxtFormat = texture_format;
            myDrp->hasColour = true;
        }
    }

    if (!isDXTPixelFormat(myDrp->destFormat))
    {
        if (myDrp->hasColour)
        {
            unsigned long dxtBufferLength = dxtBytesForDimensions(myDrp->dxtWidth, myDrp->dxtHeight, glob->type);
            if (glob->dxtBufferPool == NULL || dxtBufferLength != HapCodecBufferPoolGetBufferSize(glob->dxtBufferPool))
            {
                HapCodecBufferPoolDestroy(glob->dxtBufferPool);
                glob->dxtBufferPool = HapCodecBufferPoolCreate(dxtBufferLength);
            }
            
            myDrp->dxtBuffer = HapCodecBufferCreate(glob->dxtBufferPool);
        }

        if (myDrp->hasAlpha)
        {
            unsigned long dxtBufferLength = dxtBytesForDimensions(myDrp->dxtWidth, myDrp->dxtHeight, kHapAOnlyCodecSubType);
            if (glob->alphaBufferPool == NULL || dxtBufferLength != HapCodecBufferPoolGetBufferSize(glob->alphaBufferPool))
            {
                HapCodecBufferPoolDestroy(glob->alphaBufferPool);
                glob->alphaBufferPool = HapCodecBufferPoolCreate(dxtBufferLength);
            }

            myDrp->alphaBuffer = HapCodecBufferCreate(glob->alphaBufferPool);
        }
    }
    
    if (!isDXTPixelFormat(myDrp->destFormat) && myDrp->dxtFormat == HapTextureFormat_YCoCg_DXT5)
    {
        long convertBufferSize = glob->dxtWidth * glob->dxtHeight * 4;
        if (glob->convertBufferPool == NULL || HapCodecBufferPoolGetBufferSize(glob->convertBufferPool) != convertBufferSize)
        {
            glob->convertBufferPool = HapCodecBufferPoolCreate(convertBufferSize);
        }
        if (glob->convertBufferPool == NULL)
        {
            err = internalComponentErr;
            goto bail;
        }
        myDrp->convertBuffer = HapCodecBufferCreate(glob->convertBufferPool);
        if (myDrp->convertBuffer == NULL)
        {
            err = internalComponentErr;
            goto bail;
        }
    }
    
#ifdef HAP_GPU_DECODE
    if (!isDXTPixelFormat(myDrp->destFormat))
    {
        unsigned int texture_format = myDrp->dxtFormat;
        // The GL decoder is currently ignorant of YCoCg, we swizzle the format to one it understands
        if (texture_format == HapTextureFormat_YCoCg_DXT5) texture_format = HapCodecGLCompressedFormat_RGBA_DXT5;
        
        // Rebuild the decoder if the format has changed
        if (glob->glDecoder && texture_format != HapCodecGLGetCompressedFormat(glob->glDecoder))
        {
            HapCodecGLDestroy(glob->glDecoder);
            glob->glDecoder = NULL;
        }
        if (glob->glDecoder == NULL)
        {
            glob->glDecoder = HapCodecGLCreateDecoder(myDrp->width, myDrp->height, texture_format);
        }

#ifndef HAP_SQUISH_DECODE
        // Built without squish we're stuck if we failed to create a GL decoder, so bail
        if (glob->glDecoder == NULL)
        {
            err = internalComponentErr;
            goto bail;
        }
#endif
    }
#endif
    
    // Classify the frame so that the base codec can do the right thing.
	// It is very important to do this so that the base codec will know
	// which frames it can drop if we're in danger of falling behind.
    drp->frameType = kCodecFrameTypeKey;	
bail:
    debug_print_err(glob, err);
	return err;
}

#if defined(DEBUG)
ComponentResult Hap_DDecodeBand(HapDecompressorGlobals glob, ImageSubCodecDecompressRecord *drp, unsigned long flags HAP_ATTR_UNUSED)
#else
ComponentResult Hap_DDecodeBand(HapDecompressorGlobals glob HAP_ATTR_UNUSED, ImageSubCodecDecompressRecord *drp, unsigned long flags HAP_ATTR_UNUSED)
#endif
{
	OSErr err = noErr;
	HapDecompressRecord *myDrp = (HapDecompressRecord *)drp->userDecompressRecord;
	ICMDataProcRecordPtr dataProc = drp->dataProcRecord.dataProc ? &drp->dataProcRecord : NULL;
	
    if( dataProc )
    {
        debug_print(glob, "Using dataProc... :/\n");
        // Call the data-loading proc to ensure that we have enough data loaded.
        // NOTE: During normal movie playback, the data will be fully loaded and
        // dataProc will be NULL.  However, video frames may be transferred to other
        // containers such as PICT files and QuickTime Image files, and when drawing
        // them the data will be provided using data-loading procs.
        
        dataProc->dataProc( (Ptr *)&drp->codecData, myDrp->dataSize, dataProc->dataRefCon );
    }
    
    if (!isDXTPixelFormat(myDrp->destFormat))
    {
        if (myDrp->hasColour)
        {
            unsigned int hapResult = HapDecode(drp->codecData,
                                               myDrp->dataSize,
                                               myDrp->dxtIndex,
                                               (HapDecodeCallback)HapMTDecode,
                                               NULL,
                                               HapCodecBufferGetBaseAddress(myDrp->dxtBuffer),
                                               HapCodecBufferGetSize(myDrp->dxtBuffer),
                                               NULL,
                                               &myDrp->dxtFormat);
            if (hapResult != HapResult_No_Error)
            {
                err = (hapResult == HapResult_Bad_Frame ? codecBadDataErr : internalComponentErr);
                goto bail;
            }
            
            if (myDrp->dxtFormat == HapTextureFormat_YCoCg_DXT5)
            {
                // For now we use the dedicated YCoCgDXT decoder but we could use a regular
                // decoder if we add code to convert scaled YCoCg -> RGB
                DeCompressYCoCgDXT5((const byte *)HapCodecBufferGetBaseAddress(myDrp->dxtBuffer), (byte *)HapCodecBufferGetBaseAddress(myDrp->convertBuffer), myDrp->width, myDrp->height, myDrp->dxtWidth * 4);
            }
        }
        if (myDrp->hasAlpha)
        {
            unsigned int format;
            unsigned int hapResult = HapDecode(drp->codecData,
                                               myDrp->dataSize,
                                               myDrp->alphaIndex,
                                               (HapDecodeCallback)HapMTDecode,
                                               NULL,
                                               HapCodecBufferGetBaseAddress(myDrp->alphaBuffer),
                                               HapCodecBufferGetSize(myDrp->alphaBuffer),
                                               NULL,
                                               &format);
            if (hapResult != HapResult_No_Error)
            {
                err = (hapResult == HapResult_Bad_Frame ? codecBadDataErr : internalComponentErr);
                goto bail;
            }
        }
    }
        
	myDrp->decoded = true;
	
bail:
    debug_print_err(glob, err);
	return err;
}

// ImageCodecDrawBand
//		The base image decompressor calls your image decompressor component's ImageCodecDrawBand function
// to decompress a band or frame. Your component must implement this function. If the ImageSubCodecDecompressRecord
// structure specifies a progress function or data-loading function, the base image decompressor will never call ImageCodecDrawBand
// at interrupt time. If the ImageSubCodecDecompressRecord structure specifies a progress function, the base image decompressor
// handles codecProgressOpen and codecProgressClose calls, and your image decompressor component must not implement these functions.
// If not, the base image decompressor may call the ImageCodecDrawBand function at interrupt time.
// When the base image decompressor calls your ImageCodecDrawBand function, your component must perform the decompression specified
// by the fields of the ImageSubCodecDecompressRecord structure. The structure includes any changes your component made to it
// when performing the ImageCodecBeginBand function. If your component supports asynchronous scheduled decompression,
// it may receive more than one ImageCodecBeginBand call before receiving an ImageCodecDrawBand call.
ComponentResult Hap_DDrawBand(HapDecompressorGlobals glob, ImageSubCodecDecompressRecord *drp)
{
	ComponentResult err = noErr;
	
	HapDecompressRecord *myDrp = (HapDecompressRecord *)drp->userDecompressRecord;
    
	if( ! myDrp->decoded ) {
		// If you don't set the baseCodecShouldCallDecodeBandForAllFrames flag, or if you 
		// need QuickTime 6 compatibility, you should double-check that the frame has been decoded here,
		// and decode if necessary:
		
		err = Hap_DDecodeBand( glob, drp, 0 );
		if( err ) goto bail;
	}

    if (isDXTPixelFormat(myDrp->destFormat))
    {
        // Decompress the frame directly into the output buffer
        //
        // We only advertise the DXT type we contain, so we assume we never
        // get asked for the wrong one here

        if (myDrp->destFormat == kHapCVPixelFormat_YCoCg_DXT5_A_RGTC1)
        {
            PlanarPixmapInfoHapYCoCgA *planes = (PlanarPixmapInfoHapYCoCgA *)drp->baseAddr;
            if (planes)
            {
                unsigned int planeSize;
                void *plane;
                unsigned int format;
                unsigned int hapResult = HapResult_No_Error;

                if (myDrp->hasAlpha)
                {
                    planeSize = dxtBytesForDimensions(myDrp->dxtWidth, myDrp->dxtHeight, kHapAOnlyCodecSubType);
                    plane = drp->baseAddr + EndianS32_BtoN(planes->componentInfoARGTC1.offset);
                    hapResult = HapDecode(drp->codecData, myDrp->dataSize, myDrp->alphaIndex, (HapDecodeCallback)HapMTDecode, NULL, plane, planeSize, NULL, &format);
                }

                if (hapResult == HapResult_No_Error && myDrp->hasColour)
                {
                    planeSize = dxtBytesForDimensions(myDrp->dxtWidth, myDrp->dxtHeight, kHapYCoCgCodecSubType);
                    plane = drp->baseAddr + EndianS32_BtoN(planes->componentInfoYCoCgDXT5.offset);
                    hapResult = HapDecode(drp->codecData, myDrp->dataSize, myDrp->dxtIndex, (HapDecodeCallback)HapMTDecode, NULL, plane, planeSize, NULL, &format);
                }

                if (hapResult != HapResult_No_Error)
                {
                    err = (hapResult == HapResult_Bad_Frame ? codecBadDataErr : internalComponentErr);
                    goto bail;
                }
            }
        }
        else
        {
            unsigned int bufferSize = dxtBytesForDimensions(myDrp->dxtWidth, myDrp->dxtHeight, glob->type);
            unsigned int hapResult = HapDecode(drp->codecData, myDrp->dataSize, myDrp->dxtIndex, (HapDecodeCallback)HapMTDecode, NULL, drp->baseAddr, bufferSize, NULL, &myDrp->dxtFormat);
            if (hapResult != HapResult_No_Error)
            {
                err = (hapResult == HapResult_Bad_Frame ? codecBadDataErr : internalComponentErr);
                goto bail;
            }
        }
    }
    else
    {
        if (myDrp->hasColour)
        {
            if (myDrp->dxtFormat == HapTextureFormat_YCoCg_DXT5)
            {
                if (myDrp->destFormat == k32RGBAPixelFormat)
                {
                    ConvertCoCg_Y8888ToRGB_((uint8_t *)HapCodecBufferGetBaseAddress(myDrp->convertBuffer), (uint8_t *)drp->baseAddr, myDrp->width, myDrp->height, myDrp->dxtWidth * 4, drp->rowBytes, 1);
                }
                else
                {
                    ConvertCoCg_Y8888ToBGR_((uint8_t *)HapCodecBufferGetBaseAddress(myDrp->convertBuffer), (uint8_t *)drp->baseAddr, myDrp->width, myDrp->height, myDrp->dxtWidth * 4, drp->rowBytes, 1);
                }
            }
            else
            {
    #ifdef HAP_GPU_DECODE
                if (glob->glDecoder != NULL)
                {
                    int decodeResult = HapCodecGLDecode(glob->glDecoder,
                                                        drp->rowBytes,
                                                        (myDrp->destFormat == k32RGBAPixelFormat ? HapCodecGLPixelFormat_RGBA8 : HapCodecGLPixelFormat_BGRA8),
                                                        HapCodecBufferGetBaseAddress(myDrp->dxtBuffer),
                                                        drp->baseAddr);
                    if (decodeResult != 0)
                    {
                        err = internalComponentErr;
                        goto bail;
                    }
                }
    #endif
                
    #ifdef HAP_SQUISH_DECODE
    #ifdef HAP_GPU_DECODE
                else
                {
    #endif
                    int decompressFormat = 0;
                    switch (myDrp->dxtFormat)
                    {
                    case HapTextureFormat_RGB_DXT1:
                        decompressFormat = kHapCVPixelFormat_RGB_DXT1;
                        break;
                    case HapTextureFormat_RGBA_DXT5:
                        decompressFormat = kHapCVPixelFormat_RGBA_DXT5;
                        break;
                    default:
                        err = internalComponentErr;
                        break;
                    }
                    if (err)
                        goto bail;
                    HapCodecSquishDecode(HapCodecBufferGetBaseAddress(myDrp->dxtBuffer),
                        decompressFormat,
                        drp->baseAddr,
                        myDrp->destFormat,
                        drp->rowBytes,
                        glob->width,
                        glob->height);
    #ifdef HAP_GPU_DECODE
                }
    #endif
    #endif // HAP_SQUISH_DECODE
            }
        }

        if (myDrp->hasAlpha)
        {
            // TODO: GL decode to single-channel?
            HapCodecSquishRGTC1Decode(HapCodecBufferGetBaseAddress(myDrp->alphaBuffer), drp->baseAddr, drp->rowBytes, glob->width, glob->height);
        }
    }

bail:
    debug_print_err(glob, err);
	return err;
}

// ImageCodecEndBand
//		The ImageCodecEndBand function notifies your image decompressor component that decompression of a band has finished or
// that it was terminated by the Image Compression Manager. Your image decompressor component is not required to implement
// the ImageCodecEndBand function. The base image decompressor may call the ImageCodecEndBand function at interrupt time.
// After your image decompressor component handles an ImageCodecEndBand call, it can perform any tasks that are required
// when decompression is finished, such as disposing of data structures that are no longer needed. Because this function
// can be called at interrupt time, your component cannot use this function to dispose of data structures; this
// must occur after handling the function. The value of the result parameter should be set to noErr if the band or frame was
// drawn successfully. If it is any other value, the band or frame was not drawn.
ComponentResult Hap_DEndBand(HapDecompressorGlobals glob HAP_ATTR_UNUSED, ImageSubCodecDecompressRecord *drp, OSErr result HAP_ATTR_UNUSED, long flags HAP_ATTR_UNUSED)
{
	HapDecompressRecord *myDrp = (HapDecompressRecord *)drp->userDecompressRecord;
    HapCodecBufferReturn(myDrp->dxtBuffer);
    myDrp->dxtBuffer = NULL;
    HapCodecBufferReturn(myDrp->convertBuffer);
    myDrp->convertBuffer = NULL;
    HapCodecBufferReturn(myDrp->alphaBuffer);
    myDrp->alphaBuffer = NULL;
	return noErr;
}

// ImageCodecQueueStarting
// 		If your component supports asynchronous scheduled decompression, the base image decompressor calls your image decompressor component's
// ImageCodecQueueStarting function before decompressing the frames in the queue. Your component is not required to implement this function.
// It can implement the function if it needs to perform any tasks at this time, such as locking data structures.
// The base image decompressor never calls the ImageCodecQueueStarting function at interrupt time.
ComponentResult Hap_DQueueStarting(HapDecompressorGlobals glob)
{
#pragma unused(glob)
	
	return noErr;
}

// ImageCodecQueueStopping
//		 If your image decompressor component supports asynchronous scheduled decompression, the ImageCodecQueueStopping function notifies
// your component that the frames in the queue have been decompressed. Your component is not required to implement this function.
// After your image decompressor component handles an ImageCodecQueueStopping call, it can perform any tasks that are required when decompression
// of the frames is finished, such as disposing of data structures that are no longer needed. 
// The base image decompressor never calls the ImageCodecQueueStopping function at interrupt time.
ComponentResult Hap_DQueueStopping(HapDecompressorGlobals glob HAP_ATTR_UNUSED)
{
	return noErr;
}

// ImageCodecGetCompressedImageSize
// 		Your component receives the ImageCodecGetCompressedImageSize request whenever an application calls the ICM's GetCompressedImageSize function.
// You can use the ImageCodecGetCompressedImageSize function when you are extracting a single image from a sequence; therefore, you don't have an
// image description structure and don't know the exact size of one frame. In this case, the Image Compression Manager calls the component to determine
// the size of the data. Your component should return a long integer indicating the number of bytes of data in the compressed image. You may want to store
// the image size somewhere in the image description structure, so that you can respond to this request quickly. Only decompressors receive this request.
ComponentResult Hap_DGetCompressedImageSize(HapDecompressorGlobals glob HAP_ATTR_UNUSED,
                                                   ImageDescriptionHandle desc HAP_ATTR_UNUSED,
                                                   Ptr data HAP_ATTR_UNUSED,
                                                   long dataSize HAP_ATTR_UNUSED,
                                                   ICMDataProcRecordPtr dataProc HAP_ATTR_UNUSED,
                                                   long *size)
{
    ComponentResult err = unimpErr;
	if (size == NULL) 
		err = paramErr;
    
    debug_print_err(glob, err);
	return err;
}

// ImageCodecGetCodecInfo
//		Your component receives the ImageCodecGetCodecInfo request whenever an application calls the Image Compression Manager's GetCodecInfo function.
// Your component should return a formatted compressor information structure defining its capabilities.
// Both compressors and decompressors may receive this request.
ComponentResult Hap_DGetCodecInfo(HapDecompressorGlobals glob, CodecInfo *info)
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
