#!/bin/sh

#  build-snappy.sh
#  snappy
#
#  Created by Tom Butterworth on 27/09/2012.
#  Copyright (c) 2012 Vidvox. All rights reserved.

mkdir -p "$DERIVED_FILE_DIR/snappy-source"
mkdir -p "$DERIVED_FILE_DIR/snappy-install"
mkdir -p "$BUILT_PRODUCTS_DIR/include"
cp -r "$SRCROOT/snappy-source/" "$DERIVED_FILE_DIR/snappy-source/"
cd "$DERIVED_FILE_DIR/snappy-source/"
./configure CC="gcc -arch $ARCHS" CXX="g++ -arch $ARCHS" CPP="gcc -E" CXXCPP="g++ -E" CFLAGS="-O2 -DNDEBUG" CXXFLAGS="-O2 -DNDEBUG" LDFLAGS="-Wl,-install_name,@rpath/libsnappy.1.dylib" --enable-static=NO --disable-gtest --prefix="$DERIVED_FILE_DIR/snappy-install/" -C
make install
cp "$DERIVED_FILE_DIR/snappy-install/lib/libsnappy.1.dylib" "$BUILT_PRODUCTS_DIR"
cp -r "$DERIVED_FILE_DIR/snappy-install/include/" "$BUILT_PRODUCTS_DIR/include/"
