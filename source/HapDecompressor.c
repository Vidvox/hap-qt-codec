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

#if __APPLE_CC__
    #include <QuickTime/QuickTime.h>
#else
    #include <ConditionalMacros.h>
    #include <Endian.h>
    #include <ImageCodec.h>
#endif

#include <libkern/OSAtomic.h>
#include "HapCodecVersion.h"
#include "Utility.h"
#include "PixelFormats.h"
#include "HapCodecSubTypes.h"
#include "hap.h"
#include "Buffers.h"
#include "YCoCg.h"
#include "YCoCgDXT.h"

/*
 Defines determine the decoding methods available.
 As MacOS supports the required EXT_texture_compression_s3tc extension at least as far back as 10.5
 and the results are the same, we don't enable squish decoding at all.
 If both are enabled, squish will be used as a fallback when the extension is not available.
 */

#define HAP_GPU_DECODE

#ifndef HAP_GPU_DECODE
    #ifndef HAP_SQUISH_DECODE
        #error Neither HAP_GPU_DECODE nor HAP_SQUISH_DECODE is defined. #define one or both.
    #endif
#endif

#ifdef HAP_GPU_DECODE
    #include "HapCodecGL.h"
#endif

#ifdef HAP_SQUISH_DECODE
    #include "squish-c.h"
    #include <Accelerate/Accelerate.h>
#endif

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
    HapCodecBufferPoolRef       convertBufferPool;
#ifdef HAP_GPU_DECODE
    HapCodecGLRef               glDecoder;
#endif
#ifdef HAP_SQUISH_DECODE
    HapCodecBufferPoolRef       permuteBufferPool;
#endif
} HapDecompressorGlobalsRecord, *HapDecompressorGlobals;

typedef struct {
	long                        width;
	long                        height;
    long                        dxtWidth;
    long                        dxtHeight;
	size_t                      dataSize;
    unsigned int                texFormat;
    OSType                      destFormat;
	Boolean                     decoded;
    HapCodecBufferRef           dxtBuffer;
    HapCodecBufferRef           convertBuffer; // used for YCoCg -> RGB
#ifdef HAP_SQUISH_DECODE
    HapCodecBufferRef           permuteBuffer; // used to re-order RGB
    Boolean                     needsPermute;
#endif
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
pascal ComponentResult Hap_DOpen(HapDecompressorGlobals glob, ComponentInstance self)
{
    // Enable the following line to attach a debugger when a background helper is launched (eg for QuickTime Player X)
//    raise(SIGSTOP);
	ComponentResult err;
    
	// Allocate memory for our globals, set them up and inform the component manager that we've done so
    
#ifndef __clang_analyzer__
	glob = calloc( sizeof( HapDecompressorGlobalsRecord ), 1 );
#endif
	if( ! glob ) {
		err = memFullErr;
		goto bail;
	}

	SetComponentInstanceStorage(self, (Handle)glob);
    
    // Store the type of decompressor we are (Hap or Hap YCoCg)
    ComponentDescription componentDescription;
    GetComponentInfo((Component)self, &componentDescription, 0, 0, 0);
    
	glob->self = self;
	glob->target = self;
    glob->type = componentDescription.componentSubType;
	glob->dxtBufferPool = NULL;
    glob->convertBufferPool = NULL;
    
#ifdef HAP_GPU_DECODE
    glob->glDecoder = NULL;
#endif
    
#ifdef HAP_SQUISH_DECODE
    glob->permuteBufferPool = NULL;
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
pascal ComponentResult Hap_DClose(HapDecompressorGlobals glob, ComponentInstance self)
{
#pragma unused(self)
	// Make sure to close the base component and deallocate our storage
	if (glob)
    {
		if (glob->delegateComponent) {
			CloseComponent(glob->delegateComponent);
		}
		
        HapCodecBufferPoolDestroy(glob->dxtBufferPool);
#ifdef HAP_GPU_DECODE
        if (glob->glDecoder)
        {
            HapCodecGLDestroy(glob->glDecoder);
        }
#endif
        
        HapCodecBufferPoolDestroy(glob->convertBufferPool);
        
#ifdef HAP_SQUISH_DECODE
        HapCodecBufferPoolDestroy(glob->permuteBufferPool);
#endif
		DisposeHandle( glob->wantedDestinationPixelTypes );
		glob->wantedDestinationPixelTypes = NULL;
		
		free( glob );
	}

	return noErr;
}

// Component Version Request - Required
pascal ComponentResult Hap_DVersion(HapDecompressorGlobals glob)
{
#pragma unused(glob)
	return kHapDecompressorVersion;
}

// Component Target Request
// 		Allows another component to "target" you i.e., you call another component whenever
// you would call yourself (as a result of your component being used by another component)
pascal ComponentResult Hap_DTarget(HapDecompressorGlobals glob, ComponentInstance target)
{
	glob->target = target;
	return noErr;
}

#pragma mark-

// ImageCodecInitialize
//		The first function call that your image decompressor component receives from the base image
// decompressor is always a call to ImageCodecInitialize. In response to this call, your image decompressor
// component returns an ImageSubCodecDecompressCapabilities structure that specifies its capabilities.
pascal ComponentResult Hap_DInitialize(HapDecompressorGlobals glob, ImageSubCodecDecompressCapabilities *cap)
{
#pragma unused(glob)

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
pascal ComponentResult Hap_DPreflight(HapDecompressorGlobals glob, CodecDecompressParams *p)
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
    capabilities->extendWidth = widthRoundedUp - p->dstPixMap.bounds.right;
    capabilities->extendHeight = heightRoundedUp - p->dstPixMap.bounds.bottom;

    glob->dxtWidth = widthRoundedUp;
    glob->dxtHeight = heightRoundedUp;
    
    // TODO: cf *p->bufferGammaLevel
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
pascal ComponentResult Hap_DBeginBand(HapDecompressorGlobals glob, CodecDecompressParams *p, ImageSubCodecDecompressRecord *drp, long flags)
{
#pragma unused (flags)
	OSStatus err = noErr;
	HapDecompressRecord *myDrp = (HapDecompressRecord *)drp->userDecompressRecord;
    
	myDrp->width = (**p->imageDescription).width;
	myDrp->height = (**p->imageDescription).height;
    myDrp->dxtWidth = glob->dxtWidth;
    myDrp->dxtHeight = glob->dxtHeight;
    myDrp->convertBuffer = NULL;
    myDrp->dxtBuffer = NULL;
    
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
    // TODO: fuck what?
	myDrp->decoded = p->frameTime ? (0 != (p->frameTime->flags & icmFrameAlreadyDecoded)) : false;
    
    myDrp->destFormat = p->dstPixMap.pixelFormat;
    
    // Inspect the frame header to discover the texture format
    unsigned int hap_result = HapGetFrameTextureFormat(drp->codecData, myDrp->dataSize, &myDrp->texFormat);
    if (hap_result != HapResult_No_Error)
    {
        err = internalComponentErr;
        goto bail;
    }

    if (!isDXTPixelFormat(myDrp->destFormat))
    {
        long dxtBufferLength = dxtBytesForDimensions(myDrp->dxtWidth, myDrp->dxtHeight, glob->type);
        if (glob->dxtBufferPool == NULL || dxtBufferLength != HapCodecBufferPoolGetBufferSize(glob->dxtBufferPool))
        {
            HapCodecBufferPoolDestroy(glob->dxtBufferPool);
            glob->dxtBufferPool = HapCodecBufferPoolCreate(dxtBufferLength);
        }
        
        myDrp->dxtBuffer = HapCodecBufferCreate(glob->dxtBufferPool);
    }
    
    if (!isDXTPixelFormat(myDrp->destFormat) && myDrp->texFormat == HapTextureFormat_YCoCg_DXT5)
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
        unsigned int texture_format = myDrp->texFormat;
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

#ifdef HAP_SQUISH_DECODE
#ifdef HAP_GPU_DECODE
    if (glob->glDecoder == NULL)
    {
#endif
        if (isDXTPixelFormat(myDrp->destFormat) || (myDrp->destFormat == k32RGBAPixelFormat && myDrp->texFormat != HapTextureFormat_YCoCg_DXT5))
        {
            myDrp->needsPermute = false;
        }
        else
        {
            myDrp->needsPermute = true;
        }
        if (myDrp->needsPermute)
        {
            if (glob->permuteBufferPool == NULL || glob->dxtWidth * glob->dxtHeight * 4 != HapCodecBufferPoolGetBufferSize(glob->permuteBufferPool))
            {
                HapCodecBufferPoolDestroy(glob->permuteBufferPool);
                glob->permuteBufferPool = HapCodecBufferPoolCreate(glob->dxtWidth * glob->dxtHeight * 4);
            }
            myDrp->permuteBuffer = HapCodecBufferCreate(glob->permuteBufferPool);
        }
#ifdef HAP_GPU_DECODE
    }
#endif
#endif
    // Classify the frame so that the base codec can do the right thing.
	// It is very important to do this so that the base codec will know
	// which frames it can drop if we're in danger of falling behind.
    drp->frameType = kCodecFrameTypeKey;	
bail:
    debug_print_err(glob, err);
	return err;
}

pascal ComponentResult Hap_DDecodeBand(HapDecompressorGlobals glob, ImageSubCodecDecompressRecord *drp, unsigned long flags)
{
#pragma unused (glob, flags)
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
        unsigned int hapResult = HapDecode(drp->codecData, myDrp->dataSize, HapCodecBufferGetBaseAddress(myDrp->dxtBuffer), HapCodecBufferGetSize(myDrp->dxtBuffer), NULL, &myDrp->texFormat);
        if (hapResult != HapResult_No_Error)
        {
            err = (hapResult == HapResult_Bad_Frame ? codecBadDataErr : internalComponentErr);
            goto bail;
        }
        
        if (myDrp->texFormat == HapTextureFormat_YCoCg_DXT5)
        {
            // For now we use the dedicated YCoCgDXT decoder but we could use a regular
            // decoder if we add code to convert scaled YCoCg -> RGB
            DeCompressYCoCgDXT5(HapCodecBufferGetBaseAddress(myDrp->dxtBuffer), HapCodecBufferGetBaseAddress(myDrp->convertBuffer), myDrp->width, myDrp->height, myDrp->dxtWidth * 4);
        }
#ifdef HAP_SQUISH_DECODE
        else
        {
#ifdef HAP_GPU_DECODE
        // For GPU decoding we do it all in draw-band for now
        // may be an optimisation to start texture upload now, finish in draw-band
            if (glob->glDecoder == NULL)
            {
#endif
                if (myDrp->needsPermute && myDrp->texFormat != HapTextureFormat_YCoCg_DXT5)
                {
                    int decompressFormatFlag = 0;
                    switch (myDrp->texFormat)
                    {
                        case HapTextureFormat_RGB_DXT1:
                        case HapTextureFormat_RGBA_DXT1:
                            decompressFormatFlag = kDxt1;
                            break;
                        case HapTextureFormat_RGBA_DXT3:
                            decompressFormatFlag = kDxt3;
                            break;
                        case HapTextureFormat_RGBA_DXT5:
                            decompressFormatFlag = kDxt5;
                            break;
                        default:
                            err = internalComponentErr;
                            break;
                    }
                    if (err)
                        goto bail;
                    SquishDecompressImage(HapCodecBufferGetBaseAddress(myDrp->permuteBuffer),
                                          myDrp->dxtWidth, myDrp->dxtHeight,
                                          HapCodecBufferGetBaseAddress(myDrp->dxtBuffer), decompressFormatFlag);
                }
#ifdef HAP_GPU_DECODE
            }
#endif
        }
#endif // HAP_SQUISH_DECODE
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
pascal ComponentResult Hap_DDrawBand(HapDecompressorGlobals glob, ImageSubCodecDecompressRecord *drp)
{
	OSErr err = noErr;
	
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
        unsigned int bufferSize = dxtBytesForDimensions(myDrp->dxtWidth, myDrp->dxtHeight, glob->type);
        unsigned int hapResult = HapDecode(drp->codecData, myDrp->dataSize, drp->baseAddr, bufferSize, NULL, &myDrp->texFormat);
        if (hapResult != HapResult_No_Error)
        {
            err = (hapResult == HapResult_Bad_Frame ? codecBadDataErr : internalComponentErr);
            goto bail;
        }
    }
    else
    {
        if (myDrp->texFormat == HapTextureFormat_YCoCg_DXT5)
        {
            if (myDrp->destFormat == kCVPixelFormatType_32RGBA)
            {
                ConvertCoCg_Y8888ToRGB_(HapCodecBufferGetBaseAddress(myDrp->convertBuffer), (uint8_t *)drp->baseAddr, myDrp->width, myDrp->height, myDrp->dxtWidth * 4, drp->rowBytes);
            }
            else
            {
                ConvertCoCg_Y8888ToBGR_(HapCodecBufferGetBaseAddress(myDrp->convertBuffer), (uint8_t *)drp->baseAddr, myDrp->width, myDrp->height, myDrp->dxtWidth * 4, drp->rowBytes);
            }
        }
        else
        {
#ifdef HAP_GPU_DECODE
            if (glob->glDecoder != NULL)
            {
                HapCodecGLDecode(glob->glDecoder,
                                 drp->rowBytes,
                                 (myDrp->destFormat == k32RGBAPixelFormat ? HapCodecGLPixelFormat_RGBA8 : HapCodecGLPixelFormat_BGRA8),
                                 HapCodecBufferGetBaseAddress(myDrp->dxtBuffer),
                                 drp->baseAddr);
            }
#endif
            
#ifdef HAP_SQUISH_DECODE
#ifdef HAP_GPU_DECODE
            else
            {
#endif
                // TODO: avoid the permute stage
                if (myDrp->needsPermute)
                {
                    vImage_Buffer srcV, dstV;
                    srcV.data = HapCodecBufferGetBaseAddress(myDrp->permuteBuffer);
                    srcV.width = myDrp->dxtWidth;
                    srcV.height = myDrp->dxtHeight;
                    srcV.rowBytes = myDrp->dxtWidth * 4;
                    dstV.data = drp->baseAddr;
                    dstV.width = myDrp->dxtWidth;
                    dstV.height = myDrp->dxtHeight;
                    dstV.rowBytes = drp->rowBytes;
                    uint8_t permuteMap[] = {2, 1, 0, 3};
                    vImagePermuteChannels_ARGB8888(&srcV, &dstV, permuteMap, kvImageNoFlags);
                }
                else
                {
                    // TODO: At this stage it may happen that drp->rowBytes does not match what Squish will assume (myDrp->dxtWidth * 4)
                    // So if we end up with this code enabled, we will need to recognise that, SquishDecompressImage() to a temporary
                    // buffer then copy row-by-row to drp->baseAddr.
                    // As this code is unlikely to be used because the GL decoder works fine, I haven't added the logic for that...
#warning Need to check for a row-bytes mismatch here, see comment
                    int decompressFormatFlag = 0;
                    switch (myDrp->texFormat)
                    {
                        case HapTextureFormat_RGB_DXT1:
                        case HapTextureFormat_RGBA_DXT1:
                            decompressFormatFlag = kDxt1;
                            break;
                        case HapTextureFormat_RGBA_DXT3:
                            decompressFormatFlag = kDxt3;
                            break;
                        case HapTextureFormat_RGBA_DXT5:
                            decompressFormatFlag = kDxt5;
                            break;
                        default:
                            err = internalComponentErr;
                            break;
                    }
                    if (err)
                        goto bail;
                    
                    SquishDecompressImage((u8 *)drp->baseAddr,
                                          myDrp->dxtWidth, myDrp->dxtHeight,
                                          HapCodecBufferGetBaseAddress(myDrp->dxtBuffer), decompressFormatFlag);
                }
#ifdef HAP_GPU_DECODE
            }
#endif
#endif // HAP_SQUISH_DECODE
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
pascal ComponentResult Hap_DEndBand(HapDecompressorGlobals glob, ImageSubCodecDecompressRecord *drp, OSErr result, long flags)
{
#pragma unused(glob, result, flags)
	HapDecompressRecord *myDrp = (HapDecompressRecord *)drp->userDecompressRecord;
    HapCodecBufferReturn(myDrp->dxtBuffer);
    myDrp->dxtBuffer = NULL;
    HapCodecBufferReturn(myDrp->convertBuffer);
    myDrp->convertBuffer = NULL;
#ifdef HAP_SQUISH_DECODE
    if (myDrp->needsPermute)
    {
        HapCodecBufferReturn(myDrp->permuteBuffer);
    }
#endif
	return noErr;
}

// ImageCodecQueueStarting
// 		If your component supports asynchronous scheduled decompression, the base image decompressor calls your image decompressor component's
// ImageCodecQueueStarting function before decompressing the frames in the queue. Your component is not required to implement this function.
// It can implement the function if it needs to perform any tasks at this time, such as locking data structures.
// The base image decompressor never calls the ImageCodecQueueStarting function at interrupt time.
pascal ComponentResult Hap_DQueueStarting(HapDecompressorGlobals glob)
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
pascal ComponentResult Hap_DQueueStopping(HapDecompressorGlobals glob)
{
#pragma unused(glob)
	
	return noErr;
}

// ImageCodecGetCompressedImageSize
// 		Your component receives the ImageCodecGetCompressedImageSize request whenever an application calls the ICM's GetCompressedImageSize function.
// You can use the ImageCodecGetCompressedImageSize function when you are extracting a single image from a sequence; therefore, you don't have an
// image description structure and don't know the exact size of one frame. In this case, the Image Compression Manager calls the component to determine
// the size of the data. Your component should return a long integer indicating the number of bytes of data in the compressed image. You may want to store
// the image size somewhere in the image description structure, so that you can respond to this request quickly. Only decompressors receive this request.
pascal ComponentResult Hap_DGetCompressedImageSize(HapDecompressorGlobals glob, ImageDescriptionHandle desc, Ptr data, long dataSize, ICMDataProcRecordPtr dataProc, long *size)
{
#pragma	unused(glob, desc, data, dataSize, dataProc)
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
pascal ComponentResult Hap_DGetCodecInfo(HapDecompressorGlobals glob, CodecInfo *info)
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
