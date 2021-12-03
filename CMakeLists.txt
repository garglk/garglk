cmake_minimum_required(VERSION 3.3)

file(READ "VERSION" GARGOYLE_VERSION)
string(STRIP "${GARGOYLE_VERSION}" GARGOYLE_VERSION)

# Don't set a version here: CMake requires a version of the form
# <major>[.<minor>[.<patch>[.<tweak>]]]
# where all parts are non-negative integers. This doesn't work with
# CI builds which include the current Git revision as the version.
project(garglk)

if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT HAS_LTO)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "7" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "8")
    set(GCC7 TRUE)
endif()

# At the moment C++17 is optional, and used in only one place. In the
# near future, however, it will likely be required.
# Completely disable on macOS, since although it claims C++17 support,
# when building for older versions (as is done by gargoyle_osx.sh), not
# all C++17 features are exposed. The C++17 code is unused on macOS
# anyway, so there's no harm here.
# In addition, CMake claims C++17 support for g++7, but std::filesystem
# isn't available there. Force C++14 for g++7.
if("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES AND NOT APPLE AND NOT GCC7)
    set(CXX_VERSION 17)
else()
    set(CXX_VERSION 14)
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
        set(INTERPRETER_INSTALL_DIR "${CMAKE_INSTALL_FULL_BINDIR}")
    else()
        set(INTERPRETER_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/gargoyle")
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
