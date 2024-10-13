# xBRZ dummy interface

add_library(xbrz-null OBJECT xbrz.cpp)

cxx_standard(xbrz-null 17)

set_property(TARGET xbrz-null PROPERTY POSITION_INDEPENDENT_CODE TRUE)

target_include_directories(xbrz-null PUBLIC ../xbrz)
