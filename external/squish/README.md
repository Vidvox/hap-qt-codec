This directory contains source for squish and an Xcode project and script to build it.

Squish is available from http://code.google.com/p/libsquish/

This source was acquired at revision 44 thus:

svn export -r 44 http://libsquish.googlecode.com/svn/trunk squish-source

The test and images directories have been removed, and the following patch applied:

    diff -rupN -x .DS_Store libsquish/GNUmakefile squish-hap/GNUmakefile
    --- libsquish/GNUmakefile	2012-03-26 08:23:32.000000000 +0100
    +++ squish-hap/GNUmakefile	2013-03-24 01:01:03.000000000 +0000
    @@ -15,8 +15,8 @@ ifeq ($(HOSTTYPE),intel-pc) # intel-pc =
     endif
     
     ifeq ($(USE_SHARED),1)
    -   SOLIB = libsquish.so.$(SOVER)
    -   LIB = $(SOLIB).0
    +   SOLIB = libsquish.$(SOVER).dylib
    +   LIB = $(SOLIB)
        CPPFLAGS += -fPIC
     else
        LIB = libsquish.a
    @@ -34,7 +34,7 @@ uninstall:
     
     $(LIB): $(OBJ)
     ifeq ($(USE_SHARED),1)
    -	$(CXX) -shared -Wl,-soname,$(SOLIB) -o $@ $(OBJ)
    +	$(CXX) -dynamiclib -Wl,-install_name,@rpath/$(SOLIB) -o $@ $(OBJ)
     else
        $(AR) cr $@ $?
        ranlib $@
    diff -rupN -x .DS_Store libsquish/squish.cpp squish-hap/squish.cpp
    --- libsquish/squish.cpp	2011-08-24 11:55:59.000000000 +0100
    +++ squish-hap/squish.cpp	2013-03-24 01:03:48.000000000 +0000
    @@ -23,7 +23,7 @@
        
        -------------------------------------------------------------------------- */
        
    -#include <squish.h>
    +#include "squish.h"
     #include "colourset.h"
     #include "maths.h"
     #include "rangefit.h"

The script build-squish.sh is run from within Xcode to build a 32-bit version of the library suitable for use by the Hap codec.

