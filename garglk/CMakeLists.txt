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
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBUNDLED_FONTS")
endif()

if(WITH_BABEL)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBABEL_HANDLER")
  include_directories(../support/babel)
  set(EXTRA_LIBS ${EXTRA_LIBS} babel)
endif()

set(LINK_TYPE SHARED)

if("${GUI}" STREQUAL "GTK")
  find_package(GTK2)
  include_directories(${GTK2_INCLUDE_DIRS})
  set(EXTRA_LIBS ${EXTRA_LIBS} ${GTK2_LIBRARIES})
  set(srcs ${srcs} sysgtk.c fontfc.c)
  add_executable(gargoyle launcher.c launchgtk.c)
elseif("${GUI}" STREQUAL "MINGW32")
  add_executable(gargoyle launcher.c launchwin.c)
  include_directories(../support/freetype ../support/libpng ../support/zlib ../support/libjpeg)
  set(EXTRA_LIBS ${EXTRA_LIBS} winmm)

  # These libraries can be found with a combination of
  # include_directories and adding "png14-14", etc. to ${EXTRA_LIBS};
  # however, once winmm is also added, these can no longer be found, as
  # include_directories seems to be ignored. Linking the DLLs in
  # directly is a workaround.
  #link_directories(../support/libpng ../support/freetype ../support/libjpeg)
  #set(EXTRA_LIBS ${EXTRA_LIBS} png14-14 freetype6 jpeg-8)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/../support/freetype/freetype6.dll)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/../support/libpng/libpng14-14.dll)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/../support/libjpeg/libjpeg-8.dll)
  set(srcs ${srcs} syswin.c fontwin.c)
  set(LINK_TYPE STATIC)
else()
  message(FATAL_ERROR "Invalid GUI: ${GUI}.")
endif()

add_library(garglk ${LINK_TYPE} ${srcs})
target_link_libraries(garglk ${EXTRA_LIBS} m)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGARGLKINI=\\\"${GARGLKINI}\\\" -DGARGLKPRE=\\\"${GARGLKPRE}\\\"")

target_link_libraries(gargoyle garglk)

add_library(glkmain STATIC main.c)

install(TARGETS gargoyle DESTINATION bin)
install(TARGETS garglk DESTINATION lib)
