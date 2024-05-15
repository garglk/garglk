# hqx 1.2

add_library(hqx STATIC hq2x.c hq3x.c hq4x.c init.c)

set_property(TARGET hqx PROPERTY POSITION_INDEPENDENT_CODE TRUE)

# Clang warns that passing a uint32_t to abs() has no effect, which is
# not true: on systems with 32-bit int, any value in a uint32_t greater
# than 0x7fffffff will be converted to a negative value when it's passed
# in, and then rendered positive: for example, 0xffffffff will convert
# to -1, and be returned as 1. The call is dodgy because it expects int
# to be 32 bits (which is probable), and wrap around as in 2's
# complement (which is all but guaranteed); but it's probably going to
# work, and the warning is unnecessary.
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(hqx PRIVATE -Wno-absolute-value)
endif()

target_include_directories(hqx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
