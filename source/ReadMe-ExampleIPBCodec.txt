ExampleIPBCodec

Version 1.0

Xcode 2.1 project builds universal binary.

This sample code shows how to write a modern video compressor component
and decompressor component pair for QuickTime 7.  The compressor and
decompressor support a IPB frame pattern, which means that the
compressor is allowed to reorder frames to help improve compression
quality.

To use this sample, drag "ExampleIPBCodec.component" into the 
/Library/QuickTime folder.  Then you will see "ExampleIPB" in the 
compressor list in the Export to QuickTime Movie video settings dialog 
in QuickTime Player Pro and other applications.

In the interests of simplicity, we have deliberately chosen a very naive
encoding format, but one which allows a lossy quality-vs-bitrate
tradeoff.

Basically, the format is based on rearranging the bits of image pixels
so that the corresponding bits of each byte in an 8x8 block are grouped
together.  Priority is given to the most significant bits; when the
bit-rate budget is reached, remaining bits are left alone.  So when
encoding the number 0xFF, if we only have enough budget to encode five
bits, we'll end up reconstructing it as the number 0xF8.

In I frames, the bits left alone are zero.  P frames use the previous I
or P frame as a starting point; if the high bits of an 8x8 block match
that previous frame, they don't need to be encoded again.  Subsequent
bits that don't match are encoded as the difference (XOR) from the
starting frame.  B frames can use 8x8 blocks from the previous two I or
P frames as a starting point, selecting on a block-by-block basis.  The
codec does not perform motion compensation.

This does not represent the state of the art in encoding!  In fact, the
quality is downright lousy unless you set a very high bit rate.  On the
other hand, from an educational perspective, the loss in quality is
highly visible, because it causes pixel values to be rounded down,
causing blocks to be made darker (and greener, since the rounding
happens in YCbCr).  (This helps illustrate, for example, that B frames
achieve better quality for the same bit-rate than I or P frames alone,
since the B frames in compressed movies look best -- and in this simple
codec all frames are given the same bit budget.)

No effort has been made to optimize either the compressor or
decompressor.

The codec uses a planar YUV 4:2:0 format internally.  The compressor
converts source frames from chunky YUV 4:2:2 to planar YUV 4:2:0, and
the decompressor does the reverse conversion.  (A feature of the new
compressor component interface is the ability for compressors to request
that they always receive YUV 4:2:2 input.)

The compressor uses the familiar MPEG-2 IPB frame pattern.  However,
compressor components may use more generalized and flexible frame
patterns.  For example, you can allow more than 2 previous I and P
frames to be used by a B frame by increasing kMaxStoredFrames in
NaiveFormat.h.  More complex frame dependency and frame reordering
patterns can be created by modifying encodeSomeSourceFrames in
ExampleIPBCompressor.c.


The basic model for "new-style" video compressor components introduced
in QuickTime 7 is:
  * The ICM sets up the compressor component by calling its 
    ImageCodecPrepareToCompressFrames function.  This function should 
    create and return a dictionary describing desired characteristics of 
    source pixel buffers.  It should also record the "compressor session",
    which is its reference token when calling back into the ICM (not to be
    confused with the "compression session", which is the client's 
    reference token).  It should retain the session options object for 
    later reference, and it may modify the image description if desired.
  * Source frames are provided to the compressor component through calls 
    to its ImageCodecEncodeFrame function.  
  * The compressor allocates buffers to hold encoded frames by calling 
    ICMEncodedFrameCreateMutable and fills in the data size, sample flags,
    and frame type as applicable.  It emits encoded frames in decode order
    (the order in which frames will be presented to the decompressor) by
    calling ICMCompressorSessionEmitEncodedFrame.
  * The ImageCodecEncodeFrame function is not required to emit frames 
    before returning; the compressor component may maintain a lookahead 
    queue of source frames.  (ExampleIPBCodec uses a C array, but you can 
    use CFArray, STL, or whatever.)  
  * The compressor component's ImageCodecCompleteFrame function may be 
    called to demand that it complete encoding of a given frame.  The 
    frame does not necessarily need to be the first frame emitted, but
    it must be emitted (or dropped) before the function returns.
Documentation about individual compressor component functions can be found 
in the header file ImageCompression.h.
