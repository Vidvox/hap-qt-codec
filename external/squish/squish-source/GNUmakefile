include config

VER = 1.13
SOVER = 0

SRC = alpha.cpp clusterfit.cpp colourblock.cpp colourfit.cpp colourset.cpp maths.cpp rangefit.cpp singlecolourfit.cpp squish.cpp

HDR = alpha.h clusterfit.h colourblock.h colourfit.h colourset.h maths.h rangefit.h singlecolourfit.h squish.h
HDR += config.h simd.h simd_float.h simd_sse.h simd_ve.h singlecolourlookup.inl

OBJ = $(SRC:%.cpp=%.o)

ifeq ($(HOSTTYPE),intel-pc) # intel-pc = macos x
   USE_SHARED = 0
endif

ifeq ($(USE_SHARED),1)
   SOLIB = libsquish.$(SOVER).dylib
   LIB = $(SOLIB)
   CPPFLAGS += -fPIC
else
   LIB = libsquish.a
endif

all: $(LIB)

install: $(LIB)
	install squish.h $(INSTALL_DIR)/include
	install $(LIB) $(INSTALL_DIR)/lib

uninstall:
	$(RM) $(INSTALL_DIR)/include/squish.h
	$(RM) $(INSTALL_DIR)/lib/$(LIB)

$(LIB): $(OBJ)
ifeq ($(USE_SHARED),1)
	$(CXX) -dynamiclib -Wl,-install_name,@rpath/$(SOLIB) -o $@ $(OBJ)
else
	$(AR) cr $@ $?
	ranlib $@
endif

tgz: clean
	tar zcf libsquish-$(VER).tgz $(SRC) $(HDR) GNUmakefile config CMakeLists.txt libsquish.pro README ChangeLog Doxyfile

%.o: %.cpp
	$(CXX) $(CPPFLAGS) -I. $(CXXFLAGS) -o $@ -c $<

clean:
	$(RM) $(OBJ) $(LIB) Makefile
