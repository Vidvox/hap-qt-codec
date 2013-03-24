#!/bin/sh

#  build-squish.sh
#  squish
#
#  Created by Tom Butterworth on 29/09/2012.
#  Copyright (c) 2012 Vidvox. All rights reserved.

mkdir -p "$DERIVED_FILE_DIR/squish-source"
mkdir -p "$DERIVED_FILE_DIR/squish-install/include"
mkdir -p "$DERIVED_FILE_DIR/squish-install/lib"
mkdir -p "$BUILT_PRODUCTS_DIR/include"
cp -r "$SRCROOT/squish-source/" "$DERIVED_FILE_DIR/squish-source/"
cd "$DERIVED_FILE_DIR/squish-source/"
make install CXX="c++ -arch $ARCHS" USE_SSE=1 INSTALL_DIR="$DERIVED_FILE_DIR/squish-install/"
cp "$DERIVED_FILE_DIR/squish-install/lib/libsquish.0.dylib" "$BUILT_PRODUCTS_DIR"
cp "$DERIVED_FILE_DIR/squish-install/include/squish.h" "$BUILT_PRODUCTS_DIR/include"
