AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = $(LIBMASA_PATH)
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

bin_PROGRAMS = hipalign
hipalign_CXXFLAGS = $(HIP_CFLAGS) $(COMMONFLAGS) -malign-double -fno-strict-aliasing  
hipalign_LDADD = $(HIP_LIBS) $(COMMONFLAGS) ./src/HIPAligner.hip_o $(LIBMASA_PATH)/libmasa.a -lhiprtc -lamdhip64 -lpthread

hipalign_SOURCES = \
./src/main.cpp \
./src/hip_util.cpp \
./src/HIPAligner.cpp \
./src/HIPAligner.hip \
./src/HIPAlignerParameters.cpp 

noinst_HEADERS = \
./src/hip_util.h \
./src/HIPAligner.hpp \
./src/HIPAlignerParameters.hpp 

HIPCCFLAGS += -ffast-math
HIPCCFLAGS += --compiler-options -fno-strict-aliasing
HIPCCFLAGS += $(COMMONFLAGS)

clean-local:
	rm -f ./src/*.hip_o

.hip.hip_o:
	$(HIPCC) -o $@ -c $< $(HIPCCFLAGS)

./src/HIPAligner.hip_o: ./src/HIPAligner.hpp

dist-clone-link:
	@if test -L $(LIBMASA_PATH); then \
		mv $(LIBMASA_PATH) $(LIBMASA_PATH).link && \
		cp -r -L $(LIBMASA_PATH).link $(LIBMASA_PATH); \
	fi

dist-unclone-link:
	@if test -L $(LIBMASA_PATH).link; then \
		rm -rf $(LIBMASA_PATH) && \
		mv $(LIBMASA_PATH).link $(LIBMASA_PATH); \
	fi

dist-link: dist-clone-link dist dist-unclone-link

sw-regs:
	$(HIPCC) --save-temps -o /tmp/gpu_sw.o -c $(srcdir)/src/HIPAligner.hip $(HIPCCFLAGS)

sw-ptx:
	$(HIPCC) --genco $(srcdir)/src/HIPAligner.hip $(HIPCCFLAGS)

.PHONY: doxygen

doxygen:
	pushd . && cd doxygen && doxygen *.doxyfile && popd
	pushd . && cd $(LIBMASA_PATH)/doxygen && doxygen *.doxyfile && popd
