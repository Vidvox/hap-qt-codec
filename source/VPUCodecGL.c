//
//  VPUCodecGL.c
//  VPUCodec
//
//  Created by Tom Butterworth on 01/10/2011.
//  Copyright 2011 Tom Butterworth. All rights reserved.
//

#include "VPUCodecGL.h"
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLMacro.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

static bool openGLFormatAndTypeForFormat(VPUCGLPixelFormat pixel_format, GLenum *format_out, GLenum *type_out) __attribute__((nonnull(2,3)));
static int roundUpToMultipleOf4( int n );
static bool openGLSupportsExtension(CGLContextObj cgl_ctx, const char *extension) __attribute__((nonnull(1,2)));

enum VPUCGLCoderMode {
    VPUCGLEncoder = 0,
    VPUCGLDecoder = 1
};

struct VPUCGL {
    CGLContextObj   context;
    unsigned int    mode;
    GLuint          texture;
    GLuint          width;
    GLuint          height;
    GLenum          format;
};

// Utility to round up to a multiple of 4.
static int roundUpToMultipleOf4( int n )
{
	if( 0 != ( n & 3 ) )
		n = ( n + 3 ) & ~3;
	return n;
}

static bool openGLSupportsExtension(CGLContextObj cgl_ctx, const char *extension)

{
	// Adapted from http://www.opengl.org/resources/features/OGLextensions/
    
	const GLubyte *extensions = NULL;
	const GLubyte *start;
	GLubyte *where, *terminator;
	
	// Check for illegal spaces in extension name
	where = (GLubyte *) strchr(extension, ' ');
	if (where || *extension == '\0')
		return false;
	
	extensions = glGetString(GL_EXTENSIONS);
    
	start = extensions;
	for (;;) {
		
		where = (GLubyte *) strstr((const char *) start, extension);
		
		if (!where)
			break;
		
		terminator = where + strlen(extension);
		
		if (where == start || *(where - 1) == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return true;
		
		start = terminator;
	}
	return false;
}

static bool openGLFormatAndTypeForFormat(VPUCGLPixelFormat pixel_format, GLenum *format_out, GLenum *type_out)
{
    switch (pixel_format) {
        case VPUCGLPixelFormat_BGRA8:
            *format_out = GL_BGRA;
            *type_out = GL_UNSIGNED_INT_8_8_8_8_REV;
            return true;
        case VPUCGLPixelFormat_RGBA8:
            *format_out = GL_RGBA;
            *type_out = GL_UNSIGNED_INT_8_8_8_8_REV;
            return true;
        case VPUCGLPixelFormat_YCbCr422:
            *format_out = GL_YCBCR_422_APPLE;
            *type_out = GL_UNSIGNED_SHORT_8_8_APPLE;
            return true;
        default:
            return false;
    }
}

VPUCGLRef VPUCGLCreate(unsigned int mode, unsigned int width, unsigned int height, unsigned int compressed_format)
{
    VPUCGLRef coder = malloc(sizeof(struct VPUCGL));
    if (coder)
    {
        coder->context = NULL;
        coder->mode = mode;
        coder->texture = 0;
        coder->width = width;
        coder->height = height;
        coder->format = compressed_format;
        
        CGLPixelFormatAttribute attribs[] = {
            kCGLPFAAccelerated,
            kCGLPFAAllowOfflineRenderers,
            kCGLPFAColorSize, 32,
            kCGLPFADepthSize, 0,
            0};
        
        CGLPixelFormatObj pixelFormat;
        GLint count;
        CGLError result = CGLChoosePixelFormat(attribs, &pixelFormat, &count);
        
        if (result != kCGLNoError)
        {
            free(coder);
            return NULL;
        }
        
        result = CGLCreateContext(pixelFormat, NULL, &coder->context);
        CGLReleasePixelFormat(pixelFormat);
        
        if (result != kCGLNoError)
        {
            free(coder);
            return NULL;
        }
        CGLContextObj cgl_ctx = coder->context;
        
        /*
         Check the context supports the extensions we need
         If GL_EXT_texture_compression_s3tc is supported then GL_ARB_texture_compression
         must be too, as the former requires the latter
         */
        bool good = openGLSupportsExtension(cgl_ctx, "GL_EXT_texture_compression_s3tc");
        if (good)
        {
            glEnable(GL_TEXTURE_2D);
            glGenTextures(1, &coder->texture);
            glBindTexture(GL_TEXTURE_2D, coder->texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            
            GLuint rounded_width = roundUpToMultipleOf4(width);
            GLuint rounded_height = roundUpToMultipleOf4(height);
            
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         compressed_format,
                         rounded_width,
                         rounded_height,
                         0,
                         GL_BGRA,
                         GL_UNSIGNED_INT_8_8_8_8_REV,
                         NULL);
                        
            glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);
                                    
            glFlush();
        }
        else
        {
            VPUCGLDestroy(coder);
            coder = NULL;
        }
    }
    return coder;
}

VPUCGLRef VPUCGLCreateEncoder(unsigned int width, unsigned int height, unsigned int compressed_format)
{
    return VPUCGLCreate(VPUCGLEncoder, width, height, compressed_format);
}

VPUCGLRef VPUCGLCreateDecoder(unsigned int width, unsigned int height, unsigned int compressed_format)
{
    return VPUCGLCreate(VPUCGLDecoder, width, height, compressed_format);
}

void VPUCGLDestroy(VPUCGLRef coder)
{
    CGLContextObj cgl_ctx = coder->context;
    if (coder->texture != 0)
    {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &coder->texture);
    }
    CGLReleaseContext(coder->context);
    free(coder);
}

unsigned int VPUCGLGetWidth(VPUCGLRef coder)
{
    return coder->width;
}

unsigned int VPUCGLGetHeight(VPUCGLRef coder)
{
    return coder->height;
}

unsigned int VPUCGLGetCompressedFormat(VPUCGLRef coder)
{
    return coder->format;
}

void VPUCGLEncode(VPUCGLRef coder, unsigned int bytes_per_row, VPUCGLPixelFormat pixel_format, const void *source, void *destination)
{
    // See http://www.oldunreal.com/editing/s3tc/ARB_texture_compression.pdf
    CGLContextObj cgl_ctx = coder->context;
    
    GLenum format_gl;
    GLenum type_gl;
    bool valid = openGLFormatAndTypeForFormat(pixel_format, &format_gl, &type_gl);
    if (!valid) return;
    
    unsigned int bytes_per_pixel = (pixel_format == VPUCGLPixelFormat_YCbCr422 ? 2 : 4);
    if (bytes_per_row != coder->width * bytes_per_pixel)
    {
        // Just by the by, 10.7 introduced universal support for GL_UNPACK_ROW_BYTES_APPLE
        // (see http://www.opengl.org/registry/specs/APPLE/row_bytes.txt )
        
        glPixelStorei(GL_UNPACK_ROW_LENGTH, bytes_per_row / bytes_per_pixel);
    }
//    glTextureRangeAPPLE(GL_TEXTURE_2D, bytes_per_row * coder->height, source);
    
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    coder->width,
                    coder->height,
                    format_gl,
                    type_gl,
                    source);
    
    GLint compressed = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_ARB, &compressed);
    if (compressed == GL_TRUE)
    {
        /*
        GLint internalformat = 0;
        GLint compressed_size = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalformat);
        
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB, &compressed_size);
        */
        glGetCompressedTexImage(GL_TEXTURE_2D, 0, destination);
    }
    else
    {
        // ARGH!
    }
    glFlush();
}

void VPUCGLDecode(VPUCGLRef coder, unsigned int bytes_per_row, VPUCGLPixelFormat pixel_format, const void *source, void *destination)
{
    CGLContextObj cgl_ctx = coder->context;
    
    GLenum format_gl;
    GLenum type_gl;
    bool valid = openGLFormatAndTypeForFormat(pixel_format, &format_gl, &type_gl);
    if (!valid) return;
    
    GLuint source_buffer_size = coder->width * coder->height;
    if (coder->format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT || coder->format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
    {
        source_buffer_size *= 0.5;
    }
    
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, coder->width, coder->height, coder->format, source_buffer_size, source);
    
    if (bytes_per_row != coder->width * 4)
    {
        // Just by the by, 10.7 introduced universal support for GL_UNPACK_ROW_BYTES_APPLE
        // (see http://www.opengl.org/registry/specs/APPLE/row_bytes.txt )
        
        glPixelStorei(GL_PACK_ROW_LENGTH, bytes_per_row / 4);
    }
    
    glGetTexImage(GL_TEXTURE_2D,
                  0,
                  format_gl,
                  type_gl,
                  destination);
    glFlush();
}

