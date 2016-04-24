set(SRCS babeldata.c cgblorb.c cgdate.c cgfref.c cggestal.c cgmisc.c cgstream.c cgstyle.c cgunicod.c config.c draw.c event.c fontdata.c gi_blorb.c gi_dispa.c imgload.c imgscale.c winblank.c window.c wingfx.c wingrid.c winmask.c winpair.c wintext.c)

option(WITH_BUNDLED_FONTS "Bundle fonts." ON)

set(GARGLKINI "/etc/garglk.ini" CACHE STRING "Full path for garglk.ini.")
set(GARGLKPRE "" CACHE STRING "Binary prefix.")

if(UNIX)
  set(GUILIB "GTK" CACHE STRING "Select GUILIB. Options: GTK, EFL.")

  find_package(Fontconfig REQUIRED)
  include_directories(${FONTCONFIG_INCLUDE_DIR})
  set(EXTRA_LIBS ${EXTRA_LIBS} ${FONTCONFIG_LIBRARIES})

  find_package(Freetype REQUIRED)
  include_directories(${FREETYPE_INCLUDE_DIRS})
  set(EXTRA_LIBS ${EXTRA_LIBS} ${FREETYPE_LIBRARIES})

  find_package(JPEG REQUIRED)
  include_directories(${JPEG_INCLUDE_DIR})
  set(EXTRA_LIBS ${EXTRA_LIBS} ${JPEG_LIBRARIES})

  find_package(PNG REQUIRED)
  include_directories(${PNG_INCLUDE_DIRS})
  set(EXTRA_LIBS ${EXTRA_LIBS} ${PNG_LIBRARIES})
elseif(WIN32)
  # On Windows, the only GUILIB currently supported is MinGW, so don't
  # pretend that there are choices available.
  set(GUILIB "MINGW32")
endif()

if(WITH_SDL)
  if(UNIX)
    find_package(SDL)
    find_package(SDL_mixer)
    find_package(SDL_sound)
    if(SDL_FOUND AND SDL_MIXER_FOUND AND SDL_SOUND_FOUND)
      include_directories(${SDL_INCLUDE_DIR} ${SDL_MIXER_INCLUDE_DIRS} ${SDL_SOUND_INCLUDE_DIRS})
      # NOTE: This should be ${SDL_SOUND_LIBRARIES} but that gets clobbered in FindSDL_sound
      set(EXTRA_LIBS ${EXTRA_LIBS} ${SDL_LIBRARY} ${SDL_MIXER_LIBRARIES} ${SDL_SOUND_LIBRARY})
      set(SRCS ${SRCS} sndsdl.c)
    else()
      set(SRCS ${SRCS} sndnull.c)
    endif()
  elseif(WIN32)
    include_directories(../support/sdl ../support/sdl_sound)
    set(EXTRA_LIBS ${EXTRA_LIBS} sdl_sound)
    set(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/../support/sdl/SDL.dll)
    set(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/../support/sdl/SDL_mixer.dll)
    set(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/../support/sdl/smpeg.dll)
    set(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/../support/sdl/libvorbisfile-3.dll)
    set(SRCS ${SRCS} sndsdl.c)
  else()
    set(SRCS ${SRCS} sndnull.c)
  endif()
else()
  set(SRCS ${SRCS} sndnull.c)
endif()

if(WITH_BUNDLED_FONTS)
  set(EXTRA_DEFINITIONS ${EXTRA_DEFINITIONS} PRIVATE BUNDLED_FONTS)
endif()

if(WITH_BABEL)
  set(EXTRA_LIBS ${EXTRA_LIBS} babel)
  set(EXTRA_INCLUDES ${EXTRA_INCLUDES} PRIVATE ../support/babel)
  set(EXTRA_DEFINITIONS ${EXTRA_DEFINITIONS} PRIVATE BABEL_HANDLER)
endif()

if("${GUILIB}" STREQUAL "GTK")
  find_package(GTK2 REQUIRED)
  include_directories(${GTK2_INCLUDE_DIRS})
  set(EXTRA_LIBS ${EXTRA_LIBS} ${GTK2_LIBRARIES})
  set(SRCS ${SRCS} sysgtk.c fontfc.c)
  set(GARGOYLE_SOURCES launchgtk.c)
elseif("${GUILIB}" STREQUAL "EFL")
  find_package(Elementary REQUIRED)
  find_package(Evas REQUIRED)
  find_package(Ecore REQUIRED)

  # TODO Create modules to find these packages.
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(ECORE_X REQUIRED ecore-x)
  find_library(ECORE_X_LIBRARY
               NAMES ${ECORE_X_LIBRARIES}
               HINTS ${ECORE_X_LIBDIR} ${ECORE_X_LIBRARY_DIRS})
  pkg_check_modules(ECORE_EVAS REQUIRED ecore-evas)
  find_library(ECORE_EVAS_LIBRARY
               NAMES ${ECORE_EVAS_LIBRARIES}
               HINTS ${ECORE_EVAS_LIBDIR} ${ECORE_EVAS_LIBRARY_DIRS})

  include_directories(${ELEMENTARY_INCLUDE_DIRS} ${EVAS_INCLUDE_DIRS} ${ECORE_INCLUDE_DIRS} ${ECORE_X_INCLUDE_DIRS} ${ECORE_EVAS_INCLUDE_DIRS})
  set(EXTRA_LIBS ${EXTRA_LIBS} ${ELEMENTARY_LIBRARIES} ${EVAS_LIBRARIES} ${ECORE_LIBRARIES} ${ECORE_X_LIBRARY} ${ECORE_EVAS_LIBRARY})
  set(SRCS ${SRCS} sysefl.c fontfc.c)
  set(GARGOYLE_SOURCES launchefl.c)
elseif("${GUILIB}" STREQUAL "MINGW32")
  set(GARGOYLE_SOURCES launchwin.c)
  include_directories(../support/freetype ../support/libpng ../support/zlib ../support/libjpeg)
  set(EXTRA_LIBS ${EXTRA_LIBS} winmm)

  # These libraries can be found with a combination of
  # include_directories and adding "png14-14", etc. to ${EXTRA_LIBS};
  # however, once winmm is also added, these can no longer be found, as
  # include_directories seems to be ignored. Linking the DLLs in
  # directly is a workaround.
  #link_directories(../support/libpng ../support/freetype ../support/libjpeg)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/../support/freetype/freetype6.dll)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/../support/libpng/libpng14-14.dll)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/../support/libjpeg/libjpeg-8.dll)
  set(SRCS ${SRCS} syswin.c fontwin.c icons.rc)
else()
  message(FATAL_ERROR "Invalid GUILIB: ${GUILIB}.")
endif()

add_library(garglk ${SRCS})

target_link_libraries(garglk ${EXTRA_LIBS} m)
target_include_directories(garglk ${EXTRA_INCLUDES})
target_compile_definitions(garglk ${EXTRA_DEFINITIONS})
target_compile_definitions(garglk PRIVATE "GARGLKINI=\"${GARGLKINI}\"")

add_executable(gargoyle launcher.c ${GARGOYLE_SOURCES})

target_compile_definitions(gargoyle PRIVATE "GARGLKINI=\"${GARGLKINI}\"" "GARGLKPRE=\"${GARGLKPRE}\"")

target_link_libraries(gargoyle garglk)

add_library(glkmain STATIC main.c)

install(TARGETS gargoyle DESTINATION bin)
install(TARGETS garglk DESTINATION lib)
