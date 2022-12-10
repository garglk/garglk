add_library(c64diskimage STATIC c64diskimage.c)

c_standard(c64diskimage 11)
warnings(c64diskimage)

target_include_directories(c64diskimage INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
