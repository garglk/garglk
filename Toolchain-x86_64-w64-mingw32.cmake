set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(MINGW_TRIPLE "x86_64-w64-mingw32")
set(MINGW_PREFIX "/usr/${MINGW_TRIPLE}")

set(CMAKE_C_COMPILER "${MINGW_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${MINGW_TRIPLE}-g++")
set(CMAKE_RC_COMPILER "${MINGW_TRIPLE}-windres")

set(CMAKE_FIND_ROOT_PATH ${MINGW_PREFIX})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR} "${MINGW_PREFIX}/lib/pkgconfig")
