This directory contains source for squish and an Xcode project and script to build it.

Squish is available from http://code.google.com/p/libsquish/

This source was acquired at revision 44 thus:

    svn export -r 44 http://libsquish.googlecode.com/svn/trunk squish-source

On MacOS, the script build-squish.sh is run from within Xcode to build a 32-bit version of the library suitable for use by the Hap codec. A Visual Studio project is used to build on Windows.

Changes made to the libsquish source for Hap:

 - The test and images directories have been removed
 - GNUmakefile has been modified to build a dynamic library (for Hap this only affects the Mac build)
 - The generation of 3+clear DXT blocks is supressed as some OpenGL drivers mistreat RGB DXT1
