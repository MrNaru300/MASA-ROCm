#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
AC_PREREQ([2.71])
AC_INIT([MASA-HIPAlign],[1.0.0],[bruno.santi.oli@gmail.com],[masa-hipalign],[https://github.com/MrNaru300/MASA-ROCm])
AC_CONFIG_AUX_DIR(admin)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_FILES([Makefile])

AC_CONFIG_SUBDIRS([libs/masa-core])

AC_PROG_RANLIB

LIBMASA_PATH="libs/masa-core"

#
# Checks for programs.
#
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_MAKE_SET

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h stdlib.h string.h sys/socket.h sys/time.h unistd.h getopt.h errno.h wordexp.h],,
	AC_MSG_ERROR([Missing required header files.]))
	
# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday inet_ntoa memset mkdir pow socket strcasecmp])

# HIP Options e suporte a AMD/NVIDIA

OPTIMIZATION="-O3"

AC_ARG_ENABLE([debug],
	AS_HELP_STRING([--enable-debug],[enable cpu debug]),
	[DEBUG="-ggdb"; OPTIMIZATION=""]
)

AC_ARG_ENABLE([hip-debug],
	AS_HELP_STRING([--enable-hip-debug],[enable HIP debug]),
	[HIP_DEBUG="--debug"; OPTIMIZATION=""]
)

# Plataforma HIP: AMD or NVIDIA GPU
AC_ARG_VAR([HIP_PLATFORM], [HIP platform: amd or nvidia gpu])

if test -z "$HIP_PLATFORM"; then
	HIP_PLATFORM="amd"
fi

# Arquitetura HIP
AC_ARG_WITH([hip-arch],
	AS_HELP_STRING([--with-hip-arch=ARCH],[select one HIP architecture, e.g. gfx900, sm_80]),
	[HIP_ARCH="$withval"],
	[AS_IF([test "x$HIP_PLATFORM" = "xnvidia"], [HIP_ARCH="sm_80"], [HIP_ARCH="gfx900"])]
)

case "$HIP_PLATFORM" in
	nvidia)
		AC_MSG_NOTICE([Building for NVIDIA platform])
		HIP_ARCH_FLAGS=""
		AC_DEFINE_UNQUOTED([COMPILED_HIP_ARCH], ["$HIP_ARCH"], [CUDA architecture used in --cuda-gpu-arch hipcc parameter])
		AC_DEFINE([__HIP_PLATFORM_NVIDIA__], [1], [Define to 1 if the target platform is NVIDIA])
		COMMONFLAGS="-DHIP_PLATFORM_NVIDIA $COMMONFLAGS"
		;;
	amd)
		AC_MSG_NOTICE([Building for AMD platform])
		AC_DEFINE_UNQUOTED([COMPILED_HIP_ARCH], ["$HIP_ARCH"], [ROCm architecture used in --amdgpu-target hipcc parameter])
		AC_DEFINE([__HIP_PLATFORM_AMD__], [1], [Define to 1 if the target platform is AMD])
		HIP_ARCH_FLAGS="--amdgpu-target=$HIP_ARCH"
		;;
	*)
		AC_MSG_ERROR([Invalid HIP_PLATFORM: $HIP_PLATFORM. Must be amd or nvidia])
		;;
esac

# Conditional para Makefile.am baseado na plataforma HIP
AM_CONDITIONAL([HIP_PLATFORM_NVIDIA], [test "x$HIP_PLATFORM" = "xnvidia"])
AM_CONDITIONAL([HIP_PLATFORM_AMD], [test "x$HIP_PLATFORM" = "xamd"])


# Threads e registradores
AC_ARG_WITH([threads],
	AS_HELP_STRING([--with-threads=COUNT],[choose the number of HIP threads [default=128] (use power of 2)]),
	[HIPALIGN_THREADS="-DTHREADS_COUNT=$withval"],
	[HIPALIGN_THREADS="-DTHREADS_COUNT=128"]
)

AC_ARG_WITH([maxregcount],
	AS_HELP_STRING([--with-maxregcount=COUNT],[choose the maximum number of registers per threads [default=no]]),
	[HIP_REGISTERS="--maxrregcount $withval"],
	[HIP_REGISTERS=""]
)

#default: 64bit
AC_ARG_ENABLE([32bit],
	AS_HELP_STRING([--enable-32bit],[force compilation in 32 bit mode.]),
	[ARCH="386"; HIP_LIB_DIR="lib"; PLATFORM="-m32";
		AC_MSG_NOTICE([Enabling 32bit compilation])
	],
	[ARCH="x86_64"; HIP_LIB_DIR="lib"; PLATFORM="-m64";
		AC_MSG_NOTICE([Enabling 64bit compilation])    
	]
)

# Setup HIP paths
if test "x$HIP_PLATFORM" = "xnvidia"; then
  HIP_HOME_DEFAULT="/usr/local/cuda"
else
  HIP_HOME_DEFAULT="/opt/rocm"
fi

AC_ARG_WITH([hip],
	[AS_HELP_STRING([--with-hip=PATH],[path where HIP is installed (default=$HIP_HOME_DEFAULT)])],
	[HIP_HOME=$withval],
	[HIP_HOME=$HIP_HOME_DEFAULT]
)

AM_CONDITIONAL([COMPILE_HIP], [test "x$HIP_HOME" != "xno"])

AS_IF([test "x$HIP_HOME" != "xno"], [
	# Set HIP_CFLAGS based on platform
	AS_IF([test "x$HIP_PLATFORM" = "xnvidia"], [
		HIP_CFLAGS="-I/opt/rocm/include -I$HIP_HOME/include"
	], [
		HIP_CFLAGS="-I$HIP_HOME/include"
	])
	HIP_LIBS="-L$HIP_HOME/$HIP_LIB_DIR"
	HIP_LDFLAGS="-L$HIP_HOME/$HIP_LIB_DIR"

	# Find hipcc compiler
	AC_ARG_WITH([hipcc],
		[AS_HELP_STRING([--with-hipcc=PATH],[path where hipcc is installed (e.g. /opt/rocm/bin or /usr/local/cuda/bin)])],
		[HIPCC_PATH=$withval],
		[AS_IF([test "x$HIP_PLATFORM" = "xnvidia"], [HIPCC_PATH=/opt/rocm/bin], [HIPCC_PATH=$HIP_HOME/bin])]
	)
	AC_PATH_PROG([HIPCC],[hipcc],[],[$HIPCC_PATH])
	AS_IF([test -z "$HIPCC"], [
		AC_MSG_ERROR([HIP hipcc compiler not found in path: $HIPCC_PATH. Try running \
						  configure again with the './configure --with-hipcc=PATH' argument.])
	])

	# Check for required libraries
	save_LDFLAGS="$LDFLAGS"
	LDFLAGS="$LDFLAGS $HIP_LDFLAGS"

	AS_IF([test "x$HIP_PLATFORM" = "xnvidia"], [
		AC_CHECK_FILE([$HIP_HOME/$HIP_LIB_DIR/libcudart.so], [],
			[AC_MSG_ERROR([libcudart.so library not found in path: $HIP_HOME/$HIP_LIB_DIR. Try running \
								configure again with the './configure --with-hip=PATH' argument.])]
		)
		AC_CHECK_LIB([cudart], [cudaMalloc], [],
			[AC_MSG_ERROR([libcudart.so (cudaMalloc) was not found.])]
		)
	], [
		AC_CHECK_FILE([$HIP_HOME/$HIP_LIB_DIR/libhiprtc.so], [],
			[AC_MSG_ERROR([libhiprtc.so library not found in path: $HIP_HOME/$HIP_LIB_DIR. Try running \
								configure again with the './configure --with-hip=PATH' argument.])]
		)
		AC_CHECK_LIB([hiprtc], [hiprtcCreateProgram], [],
			[AC_MSG_ERROR([libhiprtc.so (hiprtcCreateProgram) was not found.])]
		)
	])
	LDFLAGS="$save_LDFLAGS"

	# Check for HIP headers
	save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $HIP_CFLAGS"
	AC_CHECK_HEADERS([hip/hip_runtime.h], ,
		AC_MSG_ERROR([Missing required HIP header files. Install ROCm HIP headers (e.g., /opt/rocm/include) or use --with-hip=PATH.]))
	CPPFLAGS="$save_CPPFLAGS"

	# Set HIPCCFLAGS
	HIPCCFLAGS="$HIP_CFLAGS $HIP_ARCH_FLAGS $HIP_DEBUG $HIP_REGISTERS"
])

# Common flags
COMMONFLAGS="-DUNIX $HIPALIGN_THREADS $PLATFORM $OPTIMIZATION -I$LIBMASA_PATH/src"
CXXFLAGS="$DEBUG"

# Set CXX to HIPCC if HIP is enabled
AS_IF([test "x$HIP_HOME" != "xno"], [CXX="$HIPCC"])


AC_SUBST(HIP_CFLAGS)
AC_SUBST(HIP_LIBS)
AC_SUBST(HIPCC)
AC_SUBST(HIPCCFLAGS)
AC_SUBST(CXXFLAGS)
AC_SUBST(COMMONFLAGS)

AC_SUBST(LIBMASA_PATH)

AC_OUTPUT
