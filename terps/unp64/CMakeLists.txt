add_library(unp64 STATIC 6502emu.c exo_util.c log.c unp64.c
    scanners/_Scanners.c scanners/1001card.c scanners/AbuzeCrunch.c
    scanners/ActionPacker.c scanners/ByteBoiler.c scanners/Caution.c
    scanners/CCS.c scanners/Cruel.c scanners/ECA.c scanners/Exomizer.c
    scanners/Expert.c scanners/FinalSuperComp.c scanners/Intros.c
    scanners/MasterCompressor.c scanners/Megabyte.c scanners/MrCross.c
    scanners/MrZ.c scanners/PuCrunch.c scanners/Section8.c
    scanners/TBCMultiComp.c scanners/TCScrunch.c scanners/XTC.c)

c_standard(unp64 11)
warnings(unp64)

target_include_directories(unp64 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
