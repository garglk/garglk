set (CMAKE_CXX_STANDARD 17)

add_library(unp64 STATIC 6502_emu.cpp exo_util.cpp globals.cpp unp64.cpp
    scanners/scanners.cpp scanners/abuze_crunch.cpp scanners/action_packer.cpp
    scanners/byte_boiler.cpp scanners/caution.cpp scanners/ccs.cpp scanners/cruel.cpp 
    scanners/eca.cpp scanners/exomizer.cpp scanners/expert.cpp scanners/final_super_comp.cpp
    scanners/intros.cpp scanners/master_compressor.cpp scanners/mc_cracken_comp.cpp
    scanners/megabyte.cpp scanners/mr_cross.cpp scanners/mr_z.cpp scanners/pu_crunch.cpp
    scanners/section8.cpp scanners/tbc_multicomp.cpp scanners/tcs_crunch.cpp
    scanners/time_cruncher.cpp scanners/xtc.cpp)

target_include_directories(unp64 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

warnings(unp64)

