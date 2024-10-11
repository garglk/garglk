# xBRZ dummy interface

add_library(xbrz STATIC xbrz.cpp)

cxx_standard(xbrz 17)

set_property(TARGET xbrz PROPERTY POSITION_INDEPENDENT_CODE TRUE)

target_include_directories(xbrz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
