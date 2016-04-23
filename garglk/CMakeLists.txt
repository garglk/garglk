set(SRCS babeldata.c cgblorb.c cgdate.c cgfref.c cggestal.c cgmisc.c cgstream.c
    cgstyle.c cgunicod.c config.c draw.c event.c fontdata.c gi_blorb.c
    gi_dispa.c imgload.c imgscale.c winblank.c window.c wingfx.c wingrid.c
    winmask.c winpair.c wintext.c)

option(WITH_LAUNCHER "Build the launcher (i.e. the gargoyle executable)" ON)
option(WITH_BUNDLED_FONTS "Embed default fonts into Gargoyle" ON)
option(WITH_TTS "Enable text-to-speech support" ON)
option(MINGW_USE_SYSTEM_LIBRARIES "Use system libraries instead of bundled when building with MinGW" ON)
option(BUILD_SHARED_LIBS "Build a shared libgarglk instead of a static library" ON)

set(GARGLKINI "/etc/garglk.ini" CACHE STRING "Full path for garglk.ini")
set(GARGLKPRE "" CACHE STRING "Binary prefix")

if(UNIX)
    option(WITH_QT6 "Build against Qt6 instead of Qt5" OFF)
    if(WITH_QT6)
        set(QT_VERSION "6")
    else()
        set(QT_VERSION "5")
    endif()

    set(CMAKE_AUTOMOC ON)
    find_package(PkgConfig REQUIRED)
    find_package(Qt${QT_VERSION} COMPONENTS Widgets REQUIRED CONFIG)
    # Fontconfig support isn't included with CMake till 3.14, so use pkg-config.
    pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
    find_package(Freetype REQUIRED)
    find_package(JPEG REQUIRED)
    find_package(PNG REQUIRED)
    list(APPEND GARGLK_INCLUDES ${FREETYPE_INCLUDE_DIRS} ${JPEG_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS})
    list(APPEND GARGLK_LIBS PkgConfig::FONTCONFIG ${FREETYPE_LIBRARIES} ${JPEG_LIBRARIES} ${PNG_LIBRARIES} Qt${QT_VERSION}::Widgets)
    list(APPEND GARGOYLE_LIBS Qt${QT_VERSION}::Widgets)
elseif(MINGW)
    if(MINGW_USE_SYSTEM_LIBRARIES)
        find_package(Freetype REQUIRED)
        find_package(JPEG REQUIRED)
        find_package(PNG REQUIRED)
        list(APPEND GARGLK_INCLUDES ${FREETYPE_INCLUDE_DIRS} ${JPEG_INCLUDE_DIR} ${PNG_INCLUDE_DIRS})
        list(APPEND GARGLK_LIBS ${FREETYPE_LIBRARIES} ${JPEG_LIBRARIES} ${PNG_LIBRARIES})
    endif()
else()
    message(FATAL_ERROR "Unsupported platform (${CMAKE_SYSTEM_NAME}).")
endif()

set(SOUND_SOURCE sndnull.c)
if(WITH_SDL)
    if(UNIX OR MINGW)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2 SDL2_mixer)
        list(APPEND GARGLK_LIBS PkgConfig::SDL2)
        set(SOUND_SOURCE sndsdl.c)
    endif()
endif()
list(APPEND SRCS ${SOUND_SOURCE})

if(WITH_BUNDLED_FONTS)
    list(APPEND GARGLK_DEFINITIONS PRIVATE BUNDLED_FONTS)
endif()

if(WITH_BABEL)
    list(APPEND GARGLK_LIBS babel)
    list(APPEND GARGLK_INCLUDES PRIVATE ../support/babel)
    list(APPEND GARGLK_DEFINITIONS PRIVATE BABEL_HANDLER)
endif()

if(WITH_TTS)
    if(UNIX)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SPEECH_DISPATCHER REQUIRED IMPORTED_TARGET speech-dispatcher)
        list(APPEND GARGLK_LIBS PkgConfig::SPEECH_DISPATCHER)
        list(APPEND SRCS ttsspeechd.c)
    elseif(MINGW)
        list(APPEND SRCS ttswin.c)
        list(APPEND GARGLK_LIBS sapi ole32)
    else()
        message(FATAL_ERROR "TTS requested but no implementation is available on this platform (${CMAKE_SYSTEM_NAME}).")
    endif()
else()
    list(APPEND SRCS ttsnull.c)
endif()

if(UNIX)
    find_package(PkgConfig REQUIRED)
    list(APPEND SRCS sysqt.cpp fontfc.c)

    set(GARGOYLE_SOURCES launchqt.cpp)
elseif(MINGW)
    # These libraries can be found with a combination of
    # include_directories and adding "png16-16", etc. to ${GARGLK_LIBS};
    # however, once winmm is also added, these can no longer be found, as
    # link_directories seems to be ignored. Linking the DLLs in directly
    # is a workaround.
    #link_directories(../support/libpng ../support/freetype ../support/libjpeg)
    if(NOT MINGW_USE_SYSTEM_LIBRARIES)
        list(APPEND GARGLK_INCLUDES ../support/freetype ../support/libpng ../support/zlib ../support/libjpeg)
        list(APPEND GARGLK_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../support/freetype/libfreetype-6.dll)
        list(APPEND GARGLK_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../support/libpng/libpng16-16.dll)
        list(APPEND GARGLK_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../support/libjpeg/libjpeg-8.dll)
    endif()

    list(APPEND GARGLK_LIBS winmm)

    list(APPEND SRCS syswin.c fontwin.c icons.rc)

    set(GARGOYLE_SOURCES launchwin.c)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

function(standards target)
    set_property(TARGET ${target} PROPERTY C_STANDARD 99)
    set_property(TARGET ${target} PROPERTY C_STANDARD_REQUIRED ON)
    set_property(TARGET ${target} PROPERTY C_EXTENSIONS OFF)
    set_property(TARGET ${target} PROPERTY CXX_STANDARD 14)
    set_property(TARGET ${target} PROPERTY CXX_STANDARD_REQUIRED ON)
    set_property(TARGET ${target} PROPERTY CXX_EXTENSIONS OFF)
endfunction()

add_library(garglk ${SRCS})
standards(garglk)
warnings(garglk)
target_link_libraries(garglk PRIVATE ${GARGLK_LIBS} m)
target_include_directories(garglk PRIVATE ${GARGLK_INCLUDES})
target_compile_definitions(garglk PRIVATE "GARGLKINI=\"${GARGLKINI}\"")

# This is somewhat ugly.
#
# • _XOPEN_SOURCE requests POSIX + the XSI extension
# • _POSIX_C_SOURCE is just POSIX
# • Setting _XOPEN_SOURCE (usually) sets the corresponding
#   _POSIX_C_SOURCE automatically
# • But MinGW doesn't support XSI, so _XOPEN_SOURCE does not set
#   _POSIX_C_SOURCE on MinGW
# • Setting _POSIX_C_SOURCE, in theory, is sufficient (as long as
#   Gargoyle doesn't make use of any XSI extensions)
# • But libc++ on FreeBSD ultimately causes calls to isascii() to occur,
#   and isascii() is an XSI extension
#
# The use of isascii() by libc++ has to be a bug, but it's there, so
# must be worked around. Setting both _XOPEN_SOURCE and _POSIX_C_SOURCE
# is, while ugly, at least a workable solution. If any XSI code creeps
# in, either the MinGW build will fail (and that's easy to detect) or it
# will be a function that MinGW does include, even if it doesn't
# officially support XSI (in which case it will build fine).
target_compile_definitions(garglk ${GARGLK_DEFINITIONS} "_XOPEN_SOURCE=600" "_POSIX_C_SOURCE=2001112L")

if(WITH_SDL)
    target_compile_definitions(garglk PRIVATE GARGLK_USESDL)
endif()

if(WITH_LAUNCHER)
    add_executable(gargoyle launcher.c ${GARGOYLE_SOURCES})
    standards(gargoyle)
    warnings(gargoyle)
    target_include_directories(gargoyle PRIVATE ${GARGOYLE_INCLUDES})
    target_link_libraries(gargoyle PRIVATE garglk ${GARGOYLE_LIBS})
    target_compile_definitions(gargoyle PRIVATE "GARGLKINI=\"${GARGLKINI}\"" "GARGLKPRE=\"${GARGLKPRE}\"")
    if(UNIX)
        target_compile_definitions(gargoyle PRIVATE "GARGLK_INTERPRETER_DIR=\"${CMAKE_INSTALL_PREFIX}/${INTERPRETER_INSTALL_DIR}\"")
    endif()
endif()

add_library(garglkmain STATIC main.c)

if(UNIX)
    if(WITH_LAUNCHER)
        install(TARGETS gargoyle DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()
    install(TARGETS garglk garglkmain DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(FILES gi_blorb.h glk.h glkstart.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/garglk")
    configure_file("garglk.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/garglk.pc" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/garglk.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
elseif(MINGW)
    install(TARGETS gargoyle DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
    if(BUILD_SHARED_LIBS)
        install(TARGETS garglk DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
    endif()

    if(NOT MINGW_USE_SYSTEM_LIBRARIES)
        install(FILES "${PROJECT_SOURCE_DIR}/support/libjpeg/libjpeg-8.dll" DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
        install(FILES "${PROJECT_SOURCE_DIR}/support/libpng/libpng16-16.dll" DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
        install(FILES "${PROJECT_SOURCE_DIR}/support/zlib/zlib1.dll" DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
        install(FILES "${PROJECT_SOURCE_DIR}/support/freetype/libfreetype-6.dll" DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
    endif()
endif()
