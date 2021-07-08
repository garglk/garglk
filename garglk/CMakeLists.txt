if(NOT APPLE)
    set(CMAKE_AUTOMOC ON)
endif()

option(WITH_LAUNCHER "Build the launcher (i.e. the gargoyle executable)" ON)
option(WITH_BUNDLED_FONTS "Embed default fonts into Gargoyle" ON)
option(WITH_SDL "Use SDL for sound support" ON)
option(WITH_TTS "Enable text-to-speech support" ON)
option(BUILD_SHARED_LIBS "Build a shared libgarglk instead of a static library" ON)

set(GARGLKINI "/etc/garglk.ini" CACHE STRING "Full path for garglk.ini")
set(GARGLKPRE "" CACHE STRING "Binary prefix")

add_library(garglk babeldata.c cgblorb.c cgdate.c cgfref.c cggestal.c cgmisc.c
    cgstream.c cgstyle.c cgunicod.c config.c draw.cpp event.c fontdata.c
    gi_blorb.c gi_dispa.c imgload.c imgscale.c winblank.c window.c wingfx.c
    wingrid.c winmask.c winpair.c wintext.c)
c_standard(garglk 11)
cxx_standard(garglk 14)
warnings(garglk)
target_compile_definitions(garglk PRIVATE "GARGLKINI=\"${GARGLKINI}\"")

if(APPLE)
    target_sources(garglk PRIVATE sysmac.m)
else()
    target_sources(garglk PRIVATE sysqt.cpp)
endif()

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
target_compile_definitions(garglk PRIVATE "_XOPEN_SOURCE=600" "_POSIX_C_SOURCE=200112L")

# macOS headers apparently don't define NSIG if _POSIX_C_SOURCE is
# defined unless _DARWIN_C_SOURCE is also defined, which makes sense
# since NSIG isn't POSIX; but then NSIG is _used_ by signal.h regardless
# of those same macros (i.e. it uses NSIG even if _POSIX_C_SOURCE is
# defined). The proper fix would be for signal.h to use the internal
# name for NSIG (__DARWIN_NSIG), but that's on Apple to fix, so work
# around it here.
if(APPLE)
    target_compile_definitions(garglk PRIVATE "_DARWIN_C_SOURCE")
endif()

if(WITH_LAUNCHER)
    add_executable(gargoyle launcher.c)
    target_link_libraries(gargoyle PRIVATE garglk)
    c_standard(gargoyle 11)
    cxx_standard(gargoyle 14)
    warnings(gargoyle)

    target_compile_definitions(gargoyle PRIVATE "GARGLKINI=\"${GARGLKINI}\"" "GARGLKPRE=\"${GARGLKPRE}\"")
    if(APPLE)
        target_sources(gargoyle PRIVATE launchmac.m)
    else()
        target_sources(gargoyle PRIVATE launchqt.cpp)
        if(UNIX)
            target_compile_definitions(gargoyle PRIVATE "GARGLK_INTERPRETER_DIR=\"${CMAKE_INSTALL_PREFIX}/${INTERPRETER_INSTALL_DIR}\"")
        endif()
    endif()
endif()

if(WITH_INTERPRETERS)
    add_library(garglkmain STATIC main.c)
    c_standard(garglkmain 11)
    cxx_standard(garglkmain 14)
    warnings(garglkmain)
endif()

find_package(Freetype REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
target_include_directories(garglk PRIVATE ${FREETYPE_INCLUDE_DIRS} ${JPEG_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS})
target_link_libraries(garglk PRIVATE ${FREETYPE_LIBRARIES} ${JPEG_LIBRARIES} ${PNG_LIBRARIES})

if(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_package(OpenGL REQUIRED)
    target_link_libraries(garglk PUBLIC ${COCOA_LIBRARY} ${OPENGL_LIBRARIES})
elseif(UNIX OR MINGW)
    option(WITH_QT6 "Build against Qt6 instead of Qt5" OFF)
    if(WITH_QT6)
        set(QT_VERSION "6")
    else()
        set(QT_VERSION "5")
    endif()

    find_package(PkgConfig REQUIRED)
    find_package(Qt${QT_VERSION} COMPONENTS Widgets REQUIRED CONFIG)
    if(UNIX)
        # Fontconfig support isn't included with CMake till 3.14, so use pkg-config.
        pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
        target_link_libraries(garglk PRIVATE PkgConfig::FONTCONFIG)
    endif()
    target_link_libraries(garglk PRIVATE Qt${QT_VERSION}::Widgets)

    if(WITH_LAUNCHER)
        if(NOT APPLE)
            target_link_libraries(gargoyle PRIVATE Qt${QT_VERSION}::Widgets)
        endif()
    endif()
else()
    message(FATAL_ERROR "Unsupported platform (${CMAKE_SYSTEM_NAME}).")
endif()

set(SOUND_SOURCE sndnull.c)
if(WITH_SDL)
    if(UNIX OR MINGW)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2 SDL2_mixer)
        target_link_libraries(garglk PRIVATE PkgConfig::SDL2)
        set(SOUND_SOURCE sndsdl.c)
    endif()
endif()
target_sources(garglk PRIVATE ${SOUND_SOURCE})

if(WITH_BUNDLED_FONTS)
    target_compile_definitions(garglk PRIVATE BUNDLED_FONTS)
endif()

if(WITH_BABEL)
    target_link_libraries(garglk PRIVATE babel)
    target_include_directories(garglk PRIVATE ../support/babel)
    target_compile_definitions(garglk PRIVATE BABEL_HANDLER)
endif()

if(WITH_TTS)
    if(APPLE)
        target_sources(garglk PRIVATE ttsmac.m)
    elseif(UNIX)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SPEECH_DISPATCHER REQUIRED IMPORTED_TARGET speech-dispatcher)
        target_sources(garglk PRIVATE ttsspeechd.c)
        target_link_libraries(garglk PRIVATE PkgConfig::SPEECH_DISPATCHER)
    elseif(MINGW)
        target_sources(garglk PRIVATE ttswin.c)
        target_link_libraries(garglk PRIVATE sapi ole32)
    else()
        message(FATAL_ERROR "TTS requested but no implementation is available on this platform (${CMAKE_SYSTEM_NAME}).")
    endif()
else()
    target_sources(garglk PRIVATE ttsnull.c)
endif()

if(APPLE)
    target_sources(garglk PRIVATE fontmac.m)
elseif(UNIX)
    target_sources(garglk PRIVATE fontfc.c)
elseif(MINGW)
    target_sources(garglk PRIVATE fontwin.c)
    target_link_libraries(garglk PRIVATE winmm)
    target_sources(gargoyle PRIVATE icons.rc)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

if(WITH_SDL)
    target_compile_definitions(garglk PRIVATE GARGLK_USESDL)
endif()

if(MINGW OR APPLE)
    install(TARGETS gargoyle DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
    if(BUILD_SHARED_LIBS)
        install(TARGETS garglk DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
    endif()
elseif(UNIX)
    install(TARGETS garglk DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    if(WITH_LAUNCHER)
        install(TARGETS gargoyle DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()
    if(WITH_INTERPRETERS)
        install(TARGETS garglkmain DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    endif()
    install(FILES gi_blorb.h glk.h glkstart.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/garglk")
    configure_file("garglk.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/garglk.pc" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/garglk.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()
