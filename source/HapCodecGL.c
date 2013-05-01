/*
 HapCodecGL.c
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

#include "HapCodecGL.h"
#include "Utility.h"
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLMacro.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

static bool openGLFormatAndTypeForFormat(HapCodecGLPixelFormat pixel_format, GLenum *format_out, GLenum *type_out) __attribute__((nonnull(2,3)));
static bool openGLSupportsExtension(CGLContextObj cgl_ctx, const char *extension) __attribute__((nonnull(1,2)));

enum HapCodecGLCoderMode {
    HapCodecGLEncoder = 0,
    HapCodecGLDecoder = 1
};

struct HapCodecGL {
    CGLContextObj   context;
    unsigned int    mode;
    GLuint          texture;
    void            *copyBuffer;
    GLuint          texWidth;
    GLuint          texHeight;
    GLuint          width;
    GLuint          height;
    GLenum          format;
};

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

static bool openGLFormatAndTypeForFormat(HapCodecGLPixelFormat pixel_format, GLenum *format_out, GLenum *type_out)
{
    switch (pixel_format) {
        case HapCodecGLPixelFormat_BGRA8:
            *format_out = GL_BGRA;
            *type_out = GL_UNSIGNED_INT_8_8_8_8_REV;
            return true;
        case HapCodecGLPixelFormat_RGBA8:
            *format_out = GL_RGBA;
            *type_out = GL_UNSIGNED_INT_8_8_8_8_REV;
            return true;
        case HapCodecGLPixelFormat_YCbCr422:
            *format_out = GL_YCBCR_422_APPLE;
            *type_out = GL_UNSIGNED_SHORT_8_8_APPLE;
            return true;
        default:
            return false;
    }
}

static HapCodecGLRef HapCodecGLCreate(unsigned int mode, unsigned int width, unsigned int height, unsigned int compressed_format)
{
    HapCodecGLRef coder = malloc(sizeof(struct HapCodecGL));
    if (coder)
    {
        coder->context = NULL;
        coder->mode = mode;
        coder->texture = 0;
        /*
         See note in header re non-rounded buffers.
         
         In particular, glTexImage2D with NULL data followed by glTexSubImage2D produces enitirely corrupt output
         for non-rounded dimensions on Intel HD 4000.
         
         Additionally glTexImage2D with NULL data followed by glCompressedTexSubImage2D produces corrupt edge blocks
         for non-rounded dimensions on Intel HD 4000.
         
         */
        
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
        
        GLint maxTexSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
        
        coder->texWidth = roundUpToMultipleOf4(width);
        coder->texHeight = roundUpToMultipleOf4(height);
        
        if (coder->texWidth > maxTexSize)
        {
            coder->texWidth = roundDownToMultipleOf4(maxTexSize);
        }
        if (coder->texHeight > maxTexSize)
        {
            coder->texHeight = roundDownToMultipleOf4(maxTexSize);
        }
        
        coder->width = roundUpToMultipleOf4(width);
        coder->height = roundUpToMultipleOf4(height);
        coder->format = compressed_format;
        coder->copyBuffer = NULL;
        
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
            
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         compressed_format,
                         coder->texWidth,
                         coder->texHeight,
                         0,
                         GL_BGRA,
                         GL_UNSIGNED_INT_8_8_8_8_REV,
                         NULL);
                        
            glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);
                                    
            glFlush();
        }
        else
        {
            HapCodecGLDestroy(coder);
            coder = NULL;
        }
    }
    return coder;
}

HapCodecGLRef HapCodecGLCreateEncoder(unsigned int width, unsigned int height, unsigned int compressed_format)
{
    return HapCodecGLCreate(HapCodecGLEncoder, width, height, compressed_format);
}

HapCodecGLRef HapCodecGLCreateDecoder(unsigned int width, unsigned int height, unsigned int compressed_format)
{
    return HapCodecGLCreate(HapCodecGLDecoder, width, height, compressed_format);
}

void HapCodecGLDestroy(HapCodecGLRef coder)
{
    CGLContextObj cgl_ctx = coder->context;
    if (coder->texture != 0)
    {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &coder->texture);
    }
    CGLReleaseContext(coder->context);
    free(coder->copyBuffer);
    free(coder);
}

unsigned int HapCodecGLGetCompressedFormat(HapCodecGLRef coder)
{
    return coder->format;
}

int HapCodecGLEncode(HapCodecGLRef coder, unsigned int source_bytes_per_row, HapCodecGLPixelFormat pixel_format, const void *source, void *destination)
{
    // See http://www.oldunreal.com/editing/s3tc/ARB_texture_compression.pdf
    CGLContextObj cgl_ctx = coder->context;
    
    GLenum format_gl;
    GLenum type_gl;
    bool valid = openGLFormatAndTypeForFormat(pixel_format, &format_gl, &type_gl);
    if (!valid) return 1;
        
    unsigned int source_bytes_per_pixel = (pixel_format == HapCodecGLPixelFormat_YCbCr422 ? 2 : 4);
    if (source_bytes_per_row != coder->texWidth * source_bytes_per_pixel)
    {
        // Just by the by, 10.7 introduced universal support for GL_UNPACK_ROW_BYTES_APPLE
        // (see http://www.opengl.org/registry/specs/APPLE/row_bytes.txt )
        
        glPixelStorei(GL_UNPACK_ROW_LENGTH, source_bytes_per_row / source_bytes_per_pixel);
    }
    
    // A DXT row is 4 pixels high
    size_t destination_bytes_per_dxt_row = coder->width * 4;
    size_t copy_buffer_bytes_per_dxt_row = coder->texWidth * 4;
    if (coder->format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
    {
        destination_bytes_per_dxt_row /= 2;
        copy_buffer_bytes_per_dxt_row /= 2;
    }
    
        
    for (int y = 0; y < coder->height; y += coder->texHeight) {
        
        GLuint remaining_height = coder->height - y;
        GLuint tile_height = MIN(remaining_height, coder->texHeight);
        
        for (int x = 0; x < coder->width; x += coder->texWidth) {
            
            const void *tile_source = source + (y * source_bytes_per_row) + (x * source_bytes_per_pixel);
            void *tile_destination = destination + (y / 4 * destination_bytes_per_dxt_row) + (coder->format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ? x * 2 : x * 4);
            
            GLuint remaining_width = coder->width - x;
            GLuint tile_width = MIN(remaining_width, coder->texWidth);
            
    //      glTextureRangeAPPLE(GL_TEXTURE_2D, bytes_per_row * coder->height, source);
            
            glTexSubImage2D(GL_TEXTURE_2D,
                            0,
                            0,
                            0,
                            tile_width,
                            tile_height,
                            format_gl,
                            type_gl,
                            tile_source);
            
            GLint compressed = 0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_ARB, &compressed);
            if (compressed == GL_TRUE)
            {                
                if (tile_width == coder->texWidth && tile_height == coder->texHeight)
                {
                    // This is the normal case
                    
                    glGetCompressedTexImage(GL_TEXTURE_2D, 0, tile_destination);
                }
                else
                {
                    // Copy the full texture to a temporary buffer then transfer the tile's portion
                    // to the destination
                    
                    if (coder->copyBuffer == NULL && coder->texHeight != 0 && copy_buffer_bytes_per_dxt_row != 0)
                    {
                        // A buffer the size of the full texture
                        coder->copyBuffer = malloc(copy_buffer_bytes_per_dxt_row * coder->texHeight);
                    }
                    if (coder->copyBuffer == NULL)
                    {
                        return 1;
                    }
                    else
                    {
                        glGetCompressedTexImage(GL_TEXTURE_2D, 0, coder->copyBuffer);
                        
                        void *copy_source = coder->copyBuffer;
                        void *copy_destination = tile_destination;
                        size_t copy_bytes = (coder->format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ? tile_width * 2 : tile_width * 4);
                        for (int i = 0; i < tile_height; i += 4) {
                            memcpy(copy_destination, copy_source, copy_bytes);
                            copy_destination += destination_bytes_per_dxt_row;
                            copy_source += copy_buffer_bytes_per_dxt_row;
                        }
                    }
                }
            }
            else
            {
                // ARGH!
            }
        }
    }
    glFlush();
    return 0;
}

int HapCodecGLDecode(HapCodecGLRef coder, unsigned int destination_bytes_per_row, HapCodecGLPixelFormat pixel_format, const void *source, void *destination)
{
    CGLContextObj cgl_ctx = coder->context;
    
    GLenum format_gl;
    GLenum type_gl;
    bool valid = openGLFormatAndTypeForFormat(pixel_format, &format_gl, &type_gl);
    if (!valid) return 1;
    
    size_t source_bytes_per_dxt_row = coder->width * 4; // A DXT row is 4 pixels high
    if (coder->format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
    {
        source_bytes_per_dxt_row /= 2;
    }
    
    size_t destination_bytes_per_pixel = (pixel_format == HapCodecGLPixelFormat_YCbCr422 ? 2 : 4);
    
    size_t copy_buffer_bytes_per_row = destination_bytes_per_pixel * coder->texWidth;
    
    for (int y = 0; y < coder->height; y += coder->texHeight) {
        
        GLuint remaining_height = coder->height - y;
        GLuint tile_height = MIN(remaining_height, coder->texHeight);
        
        for (int x = 0; x < coder->width; x += coder->texWidth) {
            
            const void *tile_source = source + (y / 4 * source_bytes_per_dxt_row) + (coder->format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ? x * 2 : x * 4);
            void *tile_destination = destination + (y * destination_bytes_per_row) + (x * destination_bytes_per_pixel);
            
            GLuint remaining_width = coder->width - x;
            GLuint tile_width = MIN(remaining_width, coder->texWidth);
            
            size_t tile_bytes_per_dxt_row = (coder->format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ? tile_width * 2 : tile_width * 4);
            
            if (tile_bytes_per_dxt_row == source_bytes_per_dxt_row)
            {
                glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile_width, tile_height, coder->format, tile_bytes_per_dxt_row * tile_height / 4, tile_source);
            }
            else
            {
                const void *unpack_source = tile_source;
                for (int i = 0; i < tile_height; i += 4) {
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, i, tile_width, 4, coder->format, tile_bytes_per_dxt_row, unpack_source);
                    unpack_source += source_bytes_per_dxt_row;
                }
            }
            
            if (tile_width == coder->texWidth && tile_height == coder->texHeight)
            {
                // This is the normal case
                glPixelStorei(GL_PACK_ROW_LENGTH, destination_bytes_per_row / destination_bytes_per_pixel);
                
                glGetTexImage(GL_TEXTURE_2D,
                              0,
                              format_gl,
                              type_gl,
                              tile_destination);
            }
            else
            {
                // Copy the full texture to a temporary buffer then transfer the tile's portion
                // to the destination
                
                if (coder->copyBuffer == NULL && coder->texHeight != 0 && copy_buffer_bytes_per_row != 0)
                {
                    // A buffer the size of the full texture
                    coder->copyBuffer = malloc(copy_buffer_bytes_per_row * coder->texHeight);
                }
                if (coder->copyBuffer == NULL)
                {
                    return 1;
                }
                {
                    glPixelStorei(GL_PACK_ROW_LENGTH, copy_buffer_bytes_per_row / destination_bytes_per_pixel);
                    
                    glGetTexImage(GL_TEXTURE_2D,
                                  0,
                                  format_gl,
                                  type_gl,
                                  coder->copyBuffer);
                    
                    void *copy_source = coder->copyBuffer;
                    void *copy_destination = tile_destination;
                    size_t copy_bytes = tile_width * destination_bytes_per_pixel;
                    for (int i = 0; i < tile_height; i++) {
                        memcpy(copy_destination, copy_source, copy_bytes);
                        copy_destination += destination_bytes_per_row;
                        copy_source += copy_buffer_bytes_per_row;
                    }
                }
            }
        }
    }
    glFlush();
    return 0;
}

