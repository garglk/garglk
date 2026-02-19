set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ARM64)

set(CMAKE_C_COMPILER "clang-cl")
set(CMAKE_CXX_COMPILER "clang-cl")
set(CMAKE_LINKER "lld-link")
set(CMAKE_MT "llvm-mt")
set(CMAKE_RC_COMPILER "llvm-rc")

set(MSVC_SYSROOT "/usr/aarch64-pc-windows-msvc")
set(WINSDK "${MSVC_SYSROOT}/winsdk")

set(CMAKE_C_FLAGS_INIT "--target=aarch64-pc-windows-msvc -D_CRT_SECURE_NO_WARNINGS -fuse-ld=lld-link /vctoolsdir \"${WINSDK}/crt\" /winsdkdir \"${WINSDK}/sdk\"")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} /EHsc")

set(CMAKE_EXE_LINKER_FLAGS_INIT "/winsdkdir:${WINSDK}/sdk /vctoolsdir:${WINSDK}/crt /libpath:${WINSDK}/sdk/lib/um/arm64")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT}")

set(CMAKE_FIND_ROOT_PATH "${MSVC_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR} "${MSVC_SYSROOT}/lib/pkgconfig")
