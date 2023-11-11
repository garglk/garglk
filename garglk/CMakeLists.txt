include(vtkEncodeString)

configure_file("garversion.h.in" "${CMAKE_CURRENT_LIST_DIR}/garversion.h")

option(WITH_LAUNCHER "Build the launcher (i.e. the gargoyle executable)" ON)
option(BUILD_SHARED_LIBS "Build a shared libgarglk instead of a static library" ON)
option(WITH_BUNDLED_FMT "Use Gargoyle's bundled fmt library instead of the system's fmt library" OFF)

if(UNIX AND NOT APPLE)
    option(WITH_FREEDESKTOP "Install freedesktop.org application, icon, and MIME files" ON)
    set(GARGLKINI "${CMAKE_INSTALL_FULL_SYSCONFDIR}/garglk.ini")
    if(NOT APPIMAGE)
        option(INSTALL_GARGLKINI "Install an example config file to ${GARGLKINI}" OFF)
    endif()
    option(INSTALL_DEV "Install development files (headers, pkg-config file)" OFF)
endif()

if(APPLE)
    set(INTERFACE "COCOA" CACHE STRING "Interface to use (COCOA or QT)")
else()
    set(INTERFACE "QT")
endif()

set(WITH_TTS "AUTO" CACHE STRING "Enable text-to-speech support (ON/OFF/AUTO/DYNAMIC)")
set(GARGLKPRE "" CACHE STRING "Binary prefix")
set(SOUND "SDL" CACHE STRING "Backend to use for sound (SDL, QT, or none)")
set(JPEGLIB "AUTO" CACHE STRING "JPEG library to use (TURBO, IJG, or AUTO)")

if(INTERFACE STREQUAL "QT")
    option(WITH_NATIVE_FILE_DIALOGS "Use native dialogs instead of Qt dialogs" ON)
endif()

if(MSVC)
    vtk_encode_string(
        INPUT garglk.ini
        NAME garglkini
        HEADER_OUTPUT GARGLKINI_H
        SOURCE_OUTPUT GARGLKINI_CXX
        EXPORT_SYMBOL "GARGLK_API"
        EXPORT_HEADER "garglk_importexport.h"
    )
else()
    vtk_encode_string(
        INPUT garglk.ini
        NAME garglkini
        HEADER_OUTPUT GARGLKINI_H
        SOURCE_OUTPUT GARGLKINI_CXX
    )
endif()

set_property(SOURCE config.cpp launchqt.cpp PROPERTY COMPILE_DEFINITIONS GARGLKINI_H="${GARGLKINI_H}")

vtk_encode_string(
    INPUT ../themes/dark.json
    NAME theme_dark
    HEADER_OUTPUT THEME_DARK_H
    SOURCE_OUTPUT THEME_DARK_CXX
)

vtk_encode_string(
    INPUT ../themes/light.json
    NAME theme_light
    HEADER_OUTPUT THEME_LIGHT_H
    SOURCE_OUTPUT THEME_LIGHT_CXX
)

set_property(SOURCE theme.cpp PROPERTY COMPILE_DEFINITIONS THEME_DARK_H="${THEME_DARK_H}" THEME_LIGHT_H="${THEME_LIGHT_H}")

if(INTERFACE STREQUAL "QT")
    set(CMAKE_AUTOMOC ON)
    set_property(SOURCE ${GARGLKINI_CXX} ${THEME_DARK_CXX} ${THEME_DARK_H} ${THEME_LIGHT_CXX} ${THEME_LIGHT_H} PROPERTY SKIP_AUTOMOC ON)

    if(NOT WITH_NATIVE_FILE_DIALOGS)
        set_property(SOURCE sysqt.cpp launchqt.cpp PROPERTY COMPILE_DEFINITIONS GARGLK_CONFIG_NO_NATIVE_FILE_DIALOGS)
    endif()
endif()

add_library(garglk babeldata.cpp style.cpp config.cpp draw.cpp event.cpp
    garglk.cpp imgload.cpp imgscale.cpp theme.cpp winblank.cpp window.cpp
    wingfx.cpp wingrid.cpp winmask.cpp winpair.cpp wintext.cpp zbleep.cpp
    ${GARGLKINI_CXX} ${THEME_DARK_CXX} ${THEME_LIGHT_CXX}

    # These can't be a library in cheapglk/ because they contain
    # references to code in garglk, and garglk contains references to
    # cheapglk (i.e. circular dependencies). That's fine with shared
    # libraries, but will fail with static libraries. These are pulled
    # from cheapglk and kept separate logically, but they're really a
    # part of garglk itself, so add them directly to the garglk target.
    cheapglk/cgblorb.cpp cheapglk/cgdate.cpp cheapglk/cgfref.cpp
    cheapglk/cggestal.cpp cheapglk/cgmisc.cpp cheapglk/cgstream.cpp
    cheapglk/cgunicod.cpp cheapglk/gi_blorb.c cheapglk/gi_dispa.c)

target_include_directories(garglk PRIVATE .)

c_standard(garglk 11)
cxx_standard(garglk ${CXX_VERSION})
warnings(garglk)

if(UNIX AND NOT APPLE)
    target_compile_definitions(garglk PRIVATE "GARGLKINI=\"${GARGLKINI}\"")
endif()

if(APPIMAGE)
    target_compile_definitions(garglk PRIVATE GARGLK_CONFIG_APPIMAGE)
endif()

if(INTERFACE STREQUAL "COCOA")
    target_sources(garglk PRIVATE sysmac.mm)
    target_compile_options(garglk PRIVATE "-Wno-deprecated-declarations")
else()
    target_sources(garglk PRIVATE sysqt.cpp)
endif()

find_package(Freetype REQUIRED)
find_package(PNG 1.6 REQUIRED)
target_include_directories(garglk PUBLIC cheapglk PRIVATE ${FREETYPE_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS})
target_link_libraries(garglk PRIVATE ${FREETYPE_LIBRARIES} ${PNG_LIBRARIES})

if(NOT WITH_BUNDLED_FMT)
    find_package(fmt CONFIG)
endif()

function(add_fmt target)
    if(TARGET fmt::fmt)
        target_link_libraries(${target} PRIVATE fmt::fmt)
    else()
        target_compile_definitions(${target} PRIVATE GARGLK_CONFIG_BUNDLED_FMT)
    endif()
endfunction()

add_fmt(garglk)

find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(garglk PRIVATE ${MATH_LIBRARY})
endif()

add_library(garglkmain STATIC main.cpp)
cxx_standard(garglkmain ${CXX_VERSION})
warnings(garglkmain)
target_include_directories(garglkmain PRIVATE cheapglk)

# If garglk is built with PIC, do the same for garglkmain.
get_property(GARGLK_PIC TARGET garglk PROPERTY POSITION_INDEPENDENT_CODE)
set_target_properties(garglkmain PROPERTIES POSITION_INDEPENDENT_CODE "${GARGLK_PIC}")

if(WITH_LAUNCHER)
    add_executable(gargoyle WIN32 launcher.cpp)
    target_include_directories(gargoyle PRIVATE cheapglk)
    target_link_libraries(gargoyle PRIVATE garglk)
    add_fmt(gargoyle)
    cxx_standard(gargoyle ${CXX_VERSION})
    warnings(gargoyle)

    target_compile_definitions(gargoyle PRIVATE "GARGLKINI=\"${GARGLKINI}\"" "GARGLKPRE=\"${GARGLKPRE}\"")
    if(${INTERFACE} STREQUAL "COCOA")
        target_sources(gargoyle PRIVATE launchmac.mm)
        target_compile_options(gargoyle PRIVATE "-Wno-deprecated-declarations")
    else()
        target_sources(gargoyle PRIVATE launchqt.cpp)

        # For AppImage, interpreters are installed in the same directory as the
        # launcher, so don't provide GARGLK_CONFIG_INTERPRETER_DIR: in its
        # absence, the directory of the "gargoyle" binary is searched.
        #
        # For Apple, when doing a dist install, the same holds: it expects to
        # find interpreters in the same directory as the gargoyle binary.
        if(UNIX AND NOT APPIMAGE AND NOT DIST_INSTALL)
            target_compile_definitions(gargoyle PRIVATE "GARGLK_CONFIG_INTERPRETER_DIR=\"${INTERPRETER_INSTALL_DIR}\"")
        endif()
    endif()
endif()

if(${INTERFACE} STREQUAL "COCOA")
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_package(OpenGL REQUIRED)
    target_link_libraries(garglk PUBLIC ${COCOA_LIBRARY} ${OPENGL_LIBRARIES})
elseif(UNIX OR MINGW OR MSVC)
    option(WITH_QT6 "Build against Qt6 instead of Qt5" OFF)
    if(WITH_QT6)
        set(QT_VERSION "6")
    else()
        set(QT_VERSION "5")
        option(WITH_KDE "Use KDE Frameworks (improves discovery of a text editor for config file editing)" OFF)
    endif()

    find_package(Qt${QT_VERSION} COMPONENTS Widgets REQUIRED CONFIG)
    target_link_libraries(garglk PRIVATE Qt${QT_VERSION}::Widgets)

    # QDBus exists on Windows, but almost nobody will be running DBus
    # there, so using QDBus would effectively just require shipping the
    # QDBus DLL for no gain.
    if(UNIX)
        find_package(Qt${QT_VERSION} COMPONENTS DBus CONFIG)
        if(Qt${QT_VERSION}DBus_FOUND)
            target_compile_definitions(garglk PRIVATE GARGLK_CONFIG_HAS_QDBUS)
            target_link_libraries(garglk PRIVATE Qt${QT_VERSION}::DBus)
        endif()
    endif()

    if(WITH_KDE)
        if(WITH_QT6)
            message(FATAL_ERROR "KDE support requires Qt5")
        endif()

        find_package(ECM 1.0.0 REQUIRED NO_MODULE)
        set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

        # This may trigger a warning about setting a more recent required CMake
        # version. Such a warning can be safely ignored. The newest KF5 versions
        # require a more recent version of CMake than Gargoyle does, but older
        # versions do not. If Gargoyle were to require the CMake version that
        # corresponds to the most recent KF5 version, systems with older
        # combinations of CMake and KF5 would be excluded, even though they work
        # just fine.
        find_package(KF5 REQUIRED COMPONENTS KIO Service)
        target_link_libraries(garglk PRIVATE KF5::KIOWidgets KF5::Service)
        set_property(SOURCE sysqt.cpp PROPERTY COMPILE_DEFINITIONS GARGLK_CONFIG_KDE)
    endif()

    if(WITH_LAUNCHER)
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
    target_compile_definitions(garglk PRIVATE GARGLK_CONFIG_TICK)
    set(GARGLK_NEEDS_TICK TRUE CACHE INTERNAL "")
elseif("${SOUND}" STREQUAL "SDL")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2 SDL2_mixer)
    target_link_libraries(garglk PRIVATE PkgConfig::SDL2)
    target_sources(garglk PRIVATE sndsdl.cpp)
    target_compile_definitions(garglk PRIVATE GARGLK_CONFIG_SDL)
else()
    target_sources(garglk PRIVATE sndnull.cpp)
    target_compile_definitions(garglk PRIVATE GARGLK_CONFIG_NO_SOUND)
endif()

if(NOT JPEGLIB STREQUAL "AUTO")
    set(JPEG_REQUIRED "REQUIRED")
endif()

if(JPEGLIB STREQUAL "AUTO" OR JPEGLIB STREQUAL "TURBO")
    find_package(PkgConfig ${JPEG_REQUIRED})
    if(PkgConfig_FOUND)
        pkg_check_modules(TURBO ${JPEG_REQUIRED} IMPORTED_TARGET libturbojpeg>=2.0)
        if(TARGET PkgConfig::TURBO)
            target_include_directories(garglk PRIVATE PkgConfig::TURBO)
            target_link_libraries(garglk PRIVATE PkgConfig::TURBO)
            target_compile_definitions(garglk PRIVATE GARGLK_CONFIG_JPEG_TURBO)
            set(GARGLK_FOUND_JPEG TRUE)
        endif()
    endif()
endif()

if(NOT GARGLK_FOUND_JPEG AND (JPEGLIB STREQUAL "AUTO" OR JPEGLIB STREQUAL "IJG"))
    find_package(JPEG 80 ${JPEG_REQUIRED})
    if(JPEG_FOUND)
        target_include_directories(garglk PRIVATE "${JPEG_INCLUDE_DIRS}")
        target_link_libraries(garglk PRIVATE "${JPEG_LIBRARIES}")
        set(GARGLK_FOUND_JPEG TRUE)
    endif()
endif()

if(NOT GARGLK_FOUND_JPEG)
    message(FATAL_ERROR "Unable to find a usable JPEG library")
endif()

if(WITH_BABEL)
    target_link_libraries(garglk PRIVATE babel)
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

        if(NOT WITH_TTS STREQUAL "DYNAMIC")
            pkg_check_modules(SPEECH_DISPATCHER ${TTS_REQUIRED} IMPORTED_TARGET speech-dispatcher)
        endif()

        if(WITH_TTS STREQUAL "DYNAMIC")
            target_sources(garglk PRIVATE ttsspeechd.cpp)
            target_link_libraries(garglk PRIVATE ${CMAKE_DL_LIBS})
            target_compile_definitions(garglk PRIVATE "GARGLK_CONFIG_DLOPEN_LIBSPEECHD")
        elseif(SPEECH_DISPATCHER_FOUND)
            target_sources(garglk PRIVATE ttsspeechd.cpp)
            target_link_libraries(garglk PRIVATE PkgConfig::SPEECH_DISPATCHER)
        else()
            target_sources(garglk PRIVATE ttsnull.cpp)
        endif()
    elseif(MINGW OR MSVC)
        target_sources(garglk PRIVATE ttswin.cpp)
        target_link_libraries(garglk PRIVATE sapi ole32)
    else()
        message(FATAL_ERROR "TTS requested but no implementation is available on this platform (${CMAKE_SYSTEM_NAME}).")
    endif()
else()
    target_sources(garglk PRIVATE ttsnull.cpp)
endif()

if(APPLE)
    target_sources(garglk PRIVATE fontmac.mm)
elseif(UNIX)
    # Fontconfig support isn't included with CMake till 3.14, so use pkg-config.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
    target_sources(garglk PRIVATE fontfc.cpp)
    target_link_libraries(garglk PRIVATE PkgConfig::FONTCONFIG)
elseif(MINGW OR MSVC)
    target_sources(garglk PRIVATE fontwin.cpp)
    target_link_libraries(garglk PRIVATE winmm shlwapi)
    target_sources(gargoyle PRIVATE icons.rc)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

if(DIST_INSTALL)
    if(WITH_LAUNCHER)
        install(TARGETS gargoyle DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
    endif()

    if(BUILD_SHARED_LIBS)
        install(TARGETS garglk DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
    endif()
elseif(UNIX)
    if(WITH_LAUNCHER)
        install(TARGETS gargoyle DESTINATION "${CMAKE_INSTALL_BINDIR}")
        install(FILES ../gargoyle.6 DESTINATION "${CMAKE_INSTALL_MANDIR}/man6")
    endif()

    if(INSTALL_DEV)
        install(TARGETS garglk garglkmain DESTINATION "${CMAKE_INSTALL_LIBDIR}")
        install(FILES cheapglk/gi_blorb.h cheapglk/glk.h cheapglk/glkstart.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/garglk")
        configure_file("garglk.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/garglk.pc" @ONLY)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/garglk.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
    elseif(BUILD_SHARED_LIBS)
        install(TARGETS garglk DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    endif()

    if(INSTALL_GARGLKINI)
        install(FILES "garglk.ini" DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}")
    endif()

    if(WITH_FREEDESKTOP)
        install(FILES "gargoyle.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications" RENAME "io.github.garglk.Gargoyle.desktop")
        install(FILES "io.github.garglk.GargoyleEditConfig.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
        install(FILES "gargoyle-house.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons" RENAME "io.github.garglk.Gargoyle.png")
        install(FILES "interactive-fiction.xml" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/mime/packages")
        install(FILES "io.github.garglk.Gargoyle.appdata.xml" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo")

        foreach(terp adrift advsys agt alan blorb glulx hugo-image level9 magscroll tads zmachine)
            install(FILES "gargoyle-docu2.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/mimetypes" RENAME "application-x-${terp}.png")
        endforeach()
    endif()

    if(APPIMAGE)
        set(FONT_DIRECTORY "${CMAKE_INSTALL_BINDIR}")
    else()
        set(FONT_DIRECTORY "${CMAKE_INSTALL_DATADIR}/fonts/gargoyle")
        target_compile_definitions(garglk PRIVATE "GARGLK_CONFIG_FONT_PATH=\"${CMAKE_INSTALL_FULL_DATADIR}/fonts/gargoyle\"")
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

    install(FILES
        "${PROJECT_SOURCE_DIR}/fonts/unifont.otf"
        "${PROJECT_SOURCE_DIR}/fonts/unifont_upper.otf"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/io.github.garglk/Gargoyle")

    install(FILES
        "${PROJECT_SOURCE_DIR}/themes/Blue.json"
        "${PROJECT_SOURCE_DIR}/themes/Breeze Darker.json"
        "${PROJECT_SOURCE_DIR}/themes/Lectrote Dark.json"
        "${PROJECT_SOURCE_DIR}/themes/Lectrote Sepia.json"
        "${PROJECT_SOURCE_DIR}/themes/Lectrote Slate.json"
        "${PROJECT_SOURCE_DIR}/themes/Pencil.json"
        "${PROJECT_SOURCE_DIR}/themes/Zoom.json"
        "${PROJECT_SOURCE_DIR}/themes/dark.json"
        "${PROJECT_SOURCE_DIR}/themes/light.json"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/io.github.garglk/Gargoyle/themes")

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
    "${PROJECT_SOURCE_DIR}/fonts/unifont.otf"
    "${PROJECT_SOURCE_DIR}/fonts/unifont_upper.otf"
    DESTINATION "${CMAKE_BINARY_DIR}")
