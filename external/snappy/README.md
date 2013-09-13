This directory contains source for snappy and Xcode and Visual Studio projects to build it.

Snappy is available from http://code.google.com/p/snappy/

This source was acquired as release 1.1.0 from

http://code.google.com/p/snappy/downloads/detail?name=snappy-1.1.0.tar.gz

The testdata directory has been removed and the following patch applied:

    diff -rupN snappy-source/snappy.h snappy-source-hap/snappy.h
    --- snappy-source/snappy.h	2013-02-05 14:36:32.000000000 +0000
    +++ snappy-source-hap/snappy.h	2013-02-26 15:22:46.000000000 +0000
    @@ -151,7 +151,7 @@ namespace snappy {
       // Note that there might be older data around that is compressed with larger
       // block sizes, so the decompression code should not rely on the
       // non-existence of long backreferences.
    -  static const int kBlockLog = 16;
    +  static const int kBlockLog = 15;
       static const size_t kBlockSize = 1 << kBlockLog;
     
       static const int kMaxHashTableBits = 14;

Restoring snappy 1.0's 32 kB block size improves decompression performance for DXT data on i386:

| Version       | Mbit/s |
|---------------|--------|
| 1.0.5         | 7118   |
| 1.1           | 6303   |
| 1.1 (patched) | 7197   |

A custom config.h is used configured for Windows and MacOS.
