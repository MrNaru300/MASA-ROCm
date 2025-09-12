AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = $(LIBMASA_PATH)
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

bin_PROGRAMS = hipalign
if HIP_PLATFORM_NVIDIA
hipalign_CXXFLAGS = $(HIP_CFLAGS) $(COMMONFLAGS)
else
hipalign_CXXFLAGS = $(HIP_CFLAGS) $(COMMONFLAGS) -malign-double -fno-strict-aliasing
endif

# Conditional linking based on HIP platform (AMD ROCm or NVIDIA CUDA)
if HIP_PLATFORM_NVIDIA
hipalign_LDADD = $(HIP_LIBS) $(COMMONFLAGS) ./src/HIPAligner.cu_o $(LIBMASA_PATH)/libmasa.a -lcudart -lcuda -lpthread
else
hipalign_LDADD = $(HIP_LIBS) $(COMMONFLAGS) ./src/HIPAligner.hip_o $(LIBMASA_PATH)/libmasa.a -lhiprtc -lamdhip64 -lpthread
endif

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

if HIP_PLATFORM_NVIDIA
HIPCCFLAGS += -fPIC
else
HIPCCFLAGS += -fPIC -fno-strict-aliasing -ffast-math -fPIE
endif
HIPCCFLAGS += $(COMMONFLAGS)

clean-local:
	rm -f ./src/*.hip_o ./src/*.cu ./src/*.cu_o

.hip.cu:
	$(HIPCC) --cuda -o $@ $<

.cu.cu_o:
	$(NVCC) -o $@ -c $< $(HIPCCFLAGS)

./src/HIPAligner.cu_o: ./src/HIPAligner.hpp

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
