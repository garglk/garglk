include(vtkEncodeString)

configure_file("garversion.h.in" "${CMAKE_CURRENT_LIST_DIR}/garversion.h")

option(WITH_LAUNCHER "Build the launcher (i.e. the gargoyle executable)" ON)
option(BUILD_SHARED_LIBS "Build a shared libgarglk instead of a static library" ON)

if(UNIX AND NOT APPLE)
    option(WITH_FREEDESKTOP "Install freedesktop.org application, icon, and MIME files" ON)
    set(GARGLKINI "/etc/garglk.ini" CACHE STRING "Full path for garglk.ini")
endif()

if(APPLE)
    set(INTERFACE "COCOA" CACHE STRING "Interface to use (COCOA or QT)")
else()
    set(INTERFACE "QT")
endif()

set(WITH_TTS "AUTO" CACHE STRING "Enable text-to-speech support (ON/OFF/AUTO)")
set(GARGLKPRE "" CACHE STRING "Binary prefix")
set(SOUND "SDL" CACHE STRING "Backend to use for sound (SDL, QT, or none)")

if(INTERFACE STREQUAL "QT")
    option(WITH_NATIVE_FILE_DIALOGS "Use native dialogs instead of Qt dialogs" ON)
endif()

vtk_encode_string(
    INPUT garglk.ini
    NAME garglkini
    BINARY NUL_TERMINATE
    HEADER_OUTPUT GARGLKINI_H
    SOURCE_OUTPUT GARGLKINI_CXX
)

set_property(SOURCE config.cpp PROPERTY COMPILE_DEFINITIONS GARGLKINI_H="${GARGLKINI_H}")

if(INTERFACE STREQUAL "QT")
    set(CMAKE_AUTOMOC ON)
    set_property(SOURCE garglkini.cxx PROPERTY SKIP_AUTOMOC ON)

    if(NOT WITH_NATIVE_FILE_DIALOGS)
        set_property(SOURCE sysqt.cpp launchqt.cpp PROPERTY COMPILE_DEFINITIONS GARGLK_NO_NATIVE_FILE_DIALOGS)
    endif()
endif()

add_library(garglk babeldata.c style.c config.cpp draw.cpp
    event.cpp imgload.c imgscale.c winblank.c window.c wingfx.c
    wingrid.c winmask.c winpair.c wintext.c ${GARGLKINI_CXX}

    # These can't be a library in cheapglk/ because they contain
    # references to code in garglk, and garglk contains references to
    # cheapglk (i.e. circular dependencies). That's fine with shared
    # libraries, but will fail with static libraries. These are pulled
    # from cheapglk and kept separate logically, but they're really a
    # part of garglk itself, so add them directly to the garglk target.
    cheapglk/cgblorb.c cheapglk/cgdate.c cheapglk/cgfref.cpp
    cheapglk/cggestal.c cheapglk/cgmisc.c cheapglk/cgstream.c
    cheapglk/cgunicod.c cheapglk/gi_blorb.c cheapglk/gi_dispa.c)

target_include_directories(garglk PRIVATE .)

c_standard(garglk 11)
cxx_standard(garglk ${CXX_VERSION})
warnings(garglk)

if(UNIX AND NOT APPLE)
    target_compile_definitions(garglk PRIVATE "GARGLKINI=\"${GARGLKINI}\"")
endif()

if(INTERFACE STREQUAL "COCOA")
    target_sources(garglk PRIVATE sysmac.mm)
    target_compile_options(garglk PRIVATE "-Wno-deprecated-declarations")
else()
    target_sources(garglk PRIVATE sysqt.cpp)
endif()

target_compile_definitions(garglk PRIVATE "_XOPEN_SOURCE=600")

if(WITH_LAUNCHER)
    add_executable(gargoyle WIN32 launcher.cpp)
    target_include_directories(gargoyle PRIVATE cheapglk)
    target_link_libraries(gargoyle PRIVATE garglk)
    target_compile_definitions(gargoyle PRIVATE "_XOPEN_SOURCE=600")
    c_standard(gargoyle 11)
    cxx_standard(gargoyle ${CXX_VERSION})
    warnings(gargoyle)

    target_compile_definitions(gargoyle PRIVATE "GARGLKINI=\"${GARGLKINI}\"" "GARGLKPRE=\"${GARGLKPRE}\"")
    if(${INTERFACE} STREQUAL "COCOA")
        target_sources(gargoyle PRIVATE launchmac.mm)
        target_compile_options(gargoyle PRIVATE "-Wno-deprecated-declarations")
    else()
        target_sources(gargoyle PRIVATE launchqt.cpp)

        # For AppImage, interpreters are installed in the same directory as the
        # launcher, so don't provide GARGLK_INTERPRETER_DIR: in its absence, the
        # directory of the "gargoyle" binary is searched.
        #
        # For Apple, when doing a dist install, the same holds: it expects to
        # find interpreters in the same directory as the gargoyle binary.
        if(UNIX AND NOT APPIMAGE AND NOT DIST_INSTALL)
            target_compile_definitions(gargoyle PRIVATE "GARGLK_INTERPRETER_DIR=\"${INTERPRETER_INSTALL_DIR}\"")
        endif()
    endif()
endif()

add_library(garglkmain STATIC main.c)
c_standard(garglkmain 11)
cxx_standard(garglkmain ${CXX_VERSION})
warnings(garglkmain)
target_include_directories(garglkmain PRIVATE cheapglk)

find_package(Freetype REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
target_include_directories(garglk PUBLIC cheapglk PRIVATE ${FREETYPE_INCLUDE_DIRS} ${JPEG_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS})
target_link_libraries(garglk PRIVATE ${FREETYPE_LIBRARIES} ${JPEG_LIBRARIES} ${PNG_LIBRARIES})

if(${INTERFACE} STREQUAL "COCOA")
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

    find_package(Qt${QT_VERSION} COMPONENTS Widgets REQUIRED CONFIG)
    target_link_libraries(garglk PRIVATE Qt${QT_VERSION}::Widgets)

    if(WITH_LAUNCHER AND ${INTERFACE} STREQUAL "QT")
        target_link_libraries(gargoyle PRIVATE Qt${QT_VERSION}::Widgets)
    endif()
else()
    message(FATAL_ERROR "Unsupported platform (${CMAKE_SYSTEM_NAME}).")
endif()

if("${SOUND}" STREQUAL "QT")
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

if(WITH_BABEL)
    target_link_libraries(garglk PRIVATE babel)
    target_include_directories(garglk PRIVATE "${PROJECT_SOURCE_DIR}/support/babel")
    target_compile_definitions(garglk PRIVATE BABEL_HANDLER)
endif()

if(WITH_TTS)
    if(APPLE)
        target_sources(garglk PRIVATE ttsmac.mm)
    elseif(UNIX)
        find_package(PkgConfig REQUIRED)

        if(NOT WITH_TTS STREQUAL "AUTO")
            set(TTS_REQUIRED "REQUIRED")
        endif()

        pkg_check_modules(SPEECH_DISPATCHER ${TTS_REQUIRED} IMPORTED_TARGET speech-dispatcher)

        if(SPEECH_DISPATCHER_FOUND)
            target_sources(garglk PRIVATE ttsspeechd.cpp)
            target_link_libraries(garglk PRIVATE PkgConfig::SPEECH_DISPATCHER)
        else()
            target_sources(garglk PRIVATE ttsnull.c)
        endif()
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
    target_sources(garglk PRIVATE fontmac.mm)
elseif(UNIX)
    # Fontconfig support isn't included with CMake till 3.14, so use pkg-config.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
    target_sources(garglk PRIVATE fontfc.cpp)
    target_link_libraries(garglk PRIVATE PkgConfig::FONTCONFIG)
elseif(MINGW)
    target_sources(garglk PRIVATE fontwin.cpp)
    target_link_libraries(garglk PRIVATE winmm shlwapi)
    target_sources(gargoyle PRIVATE icons.rc)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

if(DIST_INSTALL)
    install(TARGETS gargoyle DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
    if(BUILD_SHARED_LIBS)
        install(TARGETS garglk DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
    endif()
elseif(UNIX)
    install(TARGETS garglk garglkmain DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    if(WITH_LAUNCHER)
        install(TARGETS gargoyle DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()
    install(FILES cheapglk/gi_blorb.h cheapglk/glk.h glkstart.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/garglk")
    configure_file("garglk.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/garglk.pc" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/garglk.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    if(WITH_FREEDESKTOP)
        install(FILES "gargoyle.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
        install(FILES "gargoyle-house.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons")
        install(FILES "interactive-fiction.xml" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/mime/packages")

        foreach(terp adrift advsys agt alan blorb glulx hugo-image level9 magscroll tads zmachine)
            install(FILES "gargoyle-docu2.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/mimetypes" RENAME "application-x-${terp}.png")
        endforeach()
    endif()

    if(APPIMAGE)
        set(FONT_DIRECTORY "${CMAKE_INSTALL_BINDIR}")
    else()
        set(FONT_DIRECTORY "${CMAKE_INSTALL_DATADIR}/fonts/gargoyle")
        target_compile_definitions(garglk PRIVATE "GARGLK_FONT_PATH=\"${CMAKE_INSTALL_FULL_DATADIR}/fonts/gargoyle\"")
    endif()

    install(FILES
        "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Mono-Bold-Italic.ttf"
        "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Mono-Bold.ttf"
        "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Mono-Italic.ttf"
        "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Mono.ttf"
        "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Serif-Bold-Italic.ttf"
        "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Serif-Bold.ttf"
        "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Serif-Italic.ttf"
        "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Serif.ttf"
        DESTINATION "${FONT_DIRECTORY}")
endif()

# For users who want to test Gargoyle by running it out of the CMake build
# directory, ensure fonts exist here. By default Gargoyle will look for the
# fonts "Gargoyle Serif" and "Gargoyle Mono", falling back to the TTF files if
# no such fonts are on the system. If Gargoyle is not installed system-wide,
# then the TTF files won't be available either. It will, as a last-ditch effort,
# look in the current directory for the fonts, so copying them here ought to
# allow Gargoyle to find them.
file(COPY
    "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Mono-Bold-Italic.ttf"
    "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Mono-Bold.ttf"
    "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Mono-Italic.ttf"
    "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Mono.ttf"
    "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Serif-Bold-Italic.ttf"
    "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Serif-Bold.ttf"
    "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Serif-Italic.ttf"
    "${PROJECT_SOURCE_DIR}/fonts/Gargoyle-Serif.ttf"
    DESTINATION "${CMAKE_BINARY_DIR}")
