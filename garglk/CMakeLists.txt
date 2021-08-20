if(NOT APPLE)
    set(CMAKE_AUTOMOC ON)
endif()

option(WITH_LAUNCHER "Build the launcher (i.e. the gargoyle executable)" ON)
option(WITH_BUNDLED_FONTS "Embed default fonts into Gargoyle" ON)
option(WITH_TTS "Enable text-to-speech support" ON)
option(BUILD_SHARED_LIBS "Build a shared libgarglk instead of a static library" ON)

set(GARGLKINI "/etc/garglk.ini" CACHE STRING "Full path for garglk.ini")
set(GARGLKPRE "" CACHE STRING "Binary prefix")
set(SOUND "SDL" CACHE STRING "Backend to use for sound (SDL, QT, or none)")

add_library(garglk babeldata.c cgblorb.c cgdate.c cgfref.c cggestal.c cgmisc.c
    cgstream.c cgstyle.c cgunicod.c config.c draw.cpp event.cpp fontdata.c
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

target_compile_definitions(garglk PRIVATE "_XOPEN_SOURCE=600")

# SDL2 defines _DARWIN_C_SOURCE to get access to a macOS-specific
# function. This is problematic, because if the SDL2 headers are
# sandwiched between standard headers, the headers included first will
# NOT see _DARWIN_C_SOURCE, while the headers after WILL see it. The
# headers after then (reasonably) assume they are in a Darwin
# environment, and can assume specific functionality exists which may
# have been masked by the earlier include. A concrete example:
#
#    #include <sys/signal.h>
#    #include <SDL_stdinc.h> // Defines _DARWIN_C_SOURCE
#    #include <signal.h>
#
# The result:
#
#    /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/signal.h:69:42: error: use of undeclared identifier 'NSIG'
#    extern __const char *__const sys_signame[NSIG];
#
# signal.h assumes NSIG is defined if _DARWIN_C_SOURCE is defined; but
# since the header that would normally define it (sys/signal.h) was
# included when _DARWIN_C_SOURCE was NOT set, NSIG was never defined.
if(APPLE)
    target_compile_definitions(garglk PRIVATE "_DARWIN_C_SOURCE")
endif()

if(WITH_LAUNCHER)
    add_executable(gargoyle launcher.cpp)
    target_link_libraries(gargoyle PRIVATE garglk)
    target_compile_definitions(gargoyle PRIVATE "_XOPEN_SOURCE=600")
    c_standard(gargoyle 11)
    cxx_standard(gargoyle 14)
    warnings(gargoyle)

    target_compile_definitions(gargoyle PRIVATE "GARGLKINI=\"${GARGLKINI}\"" "GARGLKPRE=\"${GARGLKPRE}\"")
    if(APPLE)
        target_sources(gargoyle PRIVATE launchmac.m)
    else()
        target_sources(gargoyle PRIVATE launchqt.cpp)

        # For AppImage, interpreters are installed in the same directory as the
        # launcher, so don't provide GARGLK_INTERPRETER_DIR: in its absence, the
        # directory of the "gargoyle" binary is searched.
        if(UNIX AND NOT APPIMAGE)
            target_compile_definitions(gargoyle PRIVATE "GARGLK_INTERPRETER_DIR=\"${CMAKE_INSTALL_PREFIX}/${INTERPRETER_INSTALL_DIR}\"")
        endif()
    endif()
endif()

add_library(garglkmain STATIC main.c)
c_standard(garglkmain 11)
cxx_standard(garglkmain 14)
warnings(garglkmain)

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

if("${SOUND}" STREQUAL "QT")
    if(APPLE)
        message(FATAL_ERROR "Qt sound unsupported on macOS")
    endif()
    find_package(Qt${QT_VERSION} COMPONENTS Multimedia REQUIRED CONFIG)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SOUND REQUIRED IMPORTED_TARGET sndfile libmpg123 libopenmpt)
    target_link_libraries(garglk PRIVATE Qt${QT_VERSION}::Multimedia PkgConfig::SOUND)
    target_sources(garglk PRIVATE sndqt.cpp)
    target_compile_definitions(garglk PRIVATE GARGLK_TICK)
    set(GARGLK_NEEDS_TICK TRUE CACHE INTERNAL "")
elseif("${SOUND}" STREQUAL "SDL")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2 SDL2_mixer)
    target_link_libraries(garglk PRIVATE PkgConfig::SDL2)
    target_sources(garglk PRIVATE sndsdl.c)
    target_compile_definitions(garglk PRIVATE GARGLK_USESDL)
else()
    target_sources(garglk PRIVATE sndnull.c)
endif()

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

if(MINGW OR APPLE)
    install(TARGETS gargoyle DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
    if(BUILD_SHARED_LIBS)
        install(TARGETS garglk DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
    endif()
elseif(UNIX)
    install(TARGETS garglk garglkmain DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    if(WITH_LAUNCHER)
        install(TARGETS gargoyle DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()
    install(FILES gi_blorb.h glk.h glkstart.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/garglk")
    configure_file("garglk.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/garglk.pc" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/garglk.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()
