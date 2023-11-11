cmake_minimum_required(VERSION 3.8)

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

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9")
    set(GCC_NO_CXX17 TRUE)
endif()

# At the moment C++17 is optional, and used in only one place. In the
# near future, however, it will likely be required.
# Completely disable on macOS, since although it claims C++17 support,
# when building for older versions (as is done by gargoyle_osx.sh), not
# all C++17 features are exposed. The C++17 code is unused on macOS
# anyway, so there's no harm here.
# In addition, CMake claims C++17 support for g++7, but std::filesystem
# isn't available there. In fact, proper std::filesystem support didn't
# land in gcc till version 9 (see https://gcc.gnu.org/gcc-9/changes.html),
# so disable on gcc versions prior to 9.
if("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES AND NOT APPLE AND NOT GCC_NO_CXX17)
    set(CXX_VERSION 17)
else()
    set(CXX_VERSION 14)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")

include(Compilers)

option(WITH_INTERPRETERS "Build the included interpreters" ON)
option(WITH_BABEL "Display Treaty of Babel-derived author and title if possible" ON)
option(APPIMAGE "Tweak some settings to aid in AppImage building" OFF)

if(MSVC)
    # MSVC defaults to the equivalent of "-fvisibility=hidden", which the code is not set up to support.
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    include_directories("win32compat")
    add_compile_definitions("_CRT_SECURE_NO_WARNINGS")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options("-Wno-everything" "-Wall" "-Wextra" "-ferror-limit=100")
    endif()
endif()

if(APPLE)
    option(DIST_INSTALL "Install to ${PROJECT_SOURCE_DIR}/build/dist for packaging" OFF)
elseif(MINGW)
    set(DIST_INSTALL ON)
endif()

if(UNIX AND NOT DIST_INSTALL)
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
