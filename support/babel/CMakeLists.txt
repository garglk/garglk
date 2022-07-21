add_library(babel STATIC adrift.c advsys.c agt.c alan.c babel_handler.c blorb.c
            executable.c glulx.c hugo.c ifiction.c level9.c magscrolls.c md5.c
            misc.c register.c register_ifiction.c tads.c tads2.c tads3.c
            twine.c zcode.c)

set_property(TARGET babel PROPERTY POSITION_INDEPENDENT_CODE TRUE)

target_include_directories(babel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
