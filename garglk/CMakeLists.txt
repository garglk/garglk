set(srcs babeldata.c cgblorb.c cgdate.c cgfref.c cggestal.c cgmisc.c cgstream.c cgstyle.c cgunicod.c config.c draw.c event.c fontdata.c gi_blorb.c gi_dispa.c imgload.c imgscale.c winblank.c window.c wingfx.c wingrid.c winmask.c winpair.c wintext.c)

if(UNIX)
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
endif()

if(WITH_SDL)
  find_package(SDL)
  find_package(SDL_mixer)
  find_package(SDL_sound)
  if(SDL_FOUND AND SDL_MIXER_FOUND AND SDL_SOUND_FOUND)
    include_directories(${SDL_INCLUDE_DIR} ${SDL_MIXER_INCLUDE_DIRS} ${SDL_SOUND_INCLUDE_DIRS})
    # NOTE: This should be ${SDL_SOUND_LIBRARIES} but that gets clobbered in FindSDL_sound
    set(EXTRA_LIBS ${EXTRA_LIBS} ${SDL_LIBRARY} ${SDL_MIXER_LIBRARIES} ${SDL_SOUND_LIBRARY})
    set(srcs ${srcs} sndsdl.c)
  else()
    set(srcs ${srcs} sndnull.c)
  endif()
else()
  set(srcs ${srcs} sndnull.c)
endif()

if(WITH_BUNDLED_FONTS)
  set(EXTRA_DEFINITIONS ${EXTRA_DEFINITIONS} PRIVATE BUNDLED_FONTS)
endif()

if(WITH_BABEL)
  set(EXTRA_LIBS ${EXTRA_LIBS} babel)
  set(EXTRA_INCLUDES ${EXTRA_INCLUDES} PRIVATE ../support/babel)
  set(EXTRA_DEFINITIONS ${EXTRA_DEFINITIONS} PRIVATE BABEL_HANDLER)
endif()

if("${GUI}" STREQUAL "GTK")
  find_package(GTK2)
  include_directories(${GTK2_INCLUDE_DIRS})
  set(EXTRA_LIBS ${EXTRA_LIBS} ${GTK2_LIBRARIES})
  set(srcs ${srcs} sysgtk.c fontfc.c)
  set(GARGOYLE_SOURCES launchgtk.c)
elseif("${GUI}" STREQUAL "MINGW32")
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
  set(srcs ${srcs} syswin.c fontwin.c)
else()
  message(FATAL_ERROR "Invalid GUI: ${GUI}.")
endif()

add_library(garglk ${srcs})

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
