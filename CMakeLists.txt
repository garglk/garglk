cmake_minimum_required(VERSION 3.1)

project(garglk VERSION 2017)

if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT HAS_LTO)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")

include(Compilers)

option(WITH_INTERPRETERS "Build the included interpreters" ON)
option(WITH_BABEL "Display Treaty of Babel-derived author and title if possible" ON)
option(APPIMAGE "Tweak some settings to aid in AppImage building" OFF)

if(APPLE)
    option(DIST_INSTALL "Install to ${PROJECT_SOURCE_DIR}/build/dist for packaging")
elseif(MINGW)
    set(DIST_INSTALL ON)
endif()

if(UNIX)
    include(GNUInstallDirs)
    if(APPIMAGE)
        set(INTERPRETER_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}")
    else()
        set(INTERPRETER_INSTALL_DIR "${CMAKE_INSTALL_LIBEXECDIR}/gargoyle")
    endif()
endif()

add_subdirectory(garglk)

if(WITH_INTERPRETERS)
    add_subdirectory(terps)
endif()

if(WITH_BABEL)
    add_subdirectory(support/babel)
endif()

include(FeatureSummary)
feature_summary(WHAT ALL)
