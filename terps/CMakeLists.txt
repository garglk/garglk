option(WITH_ADVSYS "Enable the advsys interpreter" ON)
option(WITH_AGILITY "Enable the agility interpreter" ON)
option(WITH_ALAN2 "Enable the alan2 interpreter" ON)
option(WITH_ALAN3 "Enable the alan3 interpreter" ON)
option(WITH_BOCFEL "Enable the bocfel interpreter" ON)
option(WITH_GIT "Enable the git interpreter" ON)
option(WITH_GLULXE "Enable the glulxe interpreter" ON)
option(WITH_HUGO "Enable the hugo interpreter" ON)
option(WITH_JACL "Enable the jacl interpreter" ON)
option(WITH_LEVEL9 "Enable the level9 interpreter" ON)
option(WITH_MAGNETIC "Enable the magnetic interpreter" ON)
option(WITH_PLUS "Enable the plus interpreter" ON)
option(WITH_SCARE "Enable the scare interpreter" ON)
option(WITH_SCOTT "Enable the scott interpreter" ON)
option(WITH_TADS "Enable the tads interpreter" ON)
option(WITH_TAYLOR "Enable the taylor interpreter" ON)

function(terp target)
    set(options POSIX LTO MATH WARNINGS GPL2)
    set(oneval CXXSTD)
    set(multival SRCS INCLUDE_DIRS MACROS LIBS)
    cmake_parse_arguments(TERP "${options}" "${oneval}" "${multival}" ${ARGN})

    add_executable(${target} WIN32 ${TERP_SRCS})
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "${GARGLKPRE}${target}")
    target_compile_definitions(${target} PRIVATE GARGLK ${TERP_MACROS})
    target_include_directories(${target} PRIVATE ${TERP_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE garglkmain ${TERP_LIBS})

    if(TERP_GPL2)
        target_link_libraries(${target} PRIVATE garglk-gpl2)
    else()
        target_link_libraries(${target} PRIVATE garglk)
    endif()

    if(MINGW OR MSVC)
        target_sources(${target} PRIVATE "${PROJECT_SOURCE_DIR}/garglk/icons.rc" "${PROJECT_SOURCE_DIR}/terps/garglk.manifest")
    endif()

    if(NOT TERP_CXXSTD)
        set(TERP_CXXSTD 98)
    endif()

    c_standard(${target} 11)
    cxx_standard(${target} ${TERP_CXXSTD})

    if(TERP_POSIX)
        target_compile_definitions(${target} PRIVATE $<$<COMPILE_LANGUAGE:C>:_XOPEN_SOURCE=600>)
    endif()
    if(TERP_LTO)
        lto(${target})
    endif()
    if(TERP_MATH AND NOT MSVC)
        target_link_libraries(${target} PRIVATE m)
    endif()
    if(TERP_WARNINGS)
        warnings(${target})
    endif()
    if(DIST_INSTALL)
        install(TARGETS ${target} DESTINATION "${PROJECT_SOURCE_DIR}/build/dist")
    elseif(UNIX)
        install(TARGETS ${target} DESTINATION "${INTERPRETER_INSTALL_DIR}")
    endif()
endfunction()

# Ideally CMake would allow multiple identical calls to add_subdirectory, but it
# doesn't, so ensure they're called only once, if needed.
if(WITH_PLUS OR WITH_SCOTT OR WITH_TAYLOR)
    add_subdirectory(c64diskimage)
endif()

if(WITH_SCOTT OR WITH_TAYLOR)
    add_subdirectory(unp64)
endif()

if(CMAKE_VERSION VERSION_LESS "3.20")
    include(TestBigEndian)
    test_big_endian(GARGLK_BIG_ENDIAN)
else()
    if(CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN")
        set(GARGLK_BIG_ENDIAN ON)
    endif()
endif()

# ------------------------------------------------------------------------------
#
# AdvSys 1.2 + ANSI + NewParser + parts of glkize
#
if(WITH_ADVSYS)
    terp(advsys
        SRCS advsys/advmsg.c advsys/advtrm.c advsys/advprs.c
        advsys/advdbs.c advsys/advint.c advsys/advjunk.c advsys/advexe.c
        advsys/glkstart.c)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang$")
        # This is a style choice in the code.
        target_compile_options(advsys PRIVATE "-Wno-parentheses")
    endif()
endif()

# ------------------------------------------------------------------------------
#
# Agility 1.1.2 with Glk
#
# Agility uses some bad voodoo to get filenames back from glk filerefs.
# We have three options:
#   1) use the bad voodoo (only works on unixes)
#   2) don't use glk file prompts (GLK_ANSI_ONLY)
#   3) use garglk extension garglk_fileref_get_name (GARGLK)
#
if(WITH_AGILITY)
    terp(agility
        SRCS agility/agtread.c agility/gamedata.c agility/util.c
        agility/agxfile.c agility/auxfile.c agility/filename.c agility/parser.c
        agility/exec.c agility/runverb.c agility/metacommand.c agility/savegame.c
        agility/debugcmd.c agility/agil.c agility/token.c agility/disassemble.c
        agility/object.c agility/interface.c agility/os_glk.c
        MACROS GLK POSIX
        GPL2)
endif()

# ------------------------------------------------------------------------------
#
# Alan interpreter 2.8.6
#
# Fixed a couple of bugs (needsp needs resetting in a few places).
# Added glk file prompts for gargoyle using garglk_fileref_get_name.
#
if(WITH_ALAN2)
    set(ALAN2_MACROS GLK)

    if(NOT GARGLK_BIG_ENDIAN)
        list(APPEND ALAN2_MACROS REVERSED)
    endif()

    terp(alan2
        SRCS alan2/arun.c alan2/main.c alan2/debug.c alan2/args.c alan2/exe.c
        alan2/inter.c alan2/parse.c alan2/rules.c alan2/stack.c alan2/decode.c
        alan2/term.c alan2/reverse.c alan2/readline.c alan2/params.c
        alan2/sysdep.c alan2/glkstart.c alan2/glkio.c alan2/alan.version.c
        MACROS ${ALAN2_MACROS}
        POSIX)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang$")
        # This is a style choice in the code.
        target_compile_options(alan2 PRIVATE "-Wno-dangling-else")
    endif()
endif()

# ------------------------------------------------------------------------------
#
# Alan interpreter 3.0beta8
#
if(WITH_ALAN3)
    terp(alan3
        SRCS alan3/alan.version.c alan3/act.c alan3/actor.c alan3/args.c
        alan3/arun.c alan3/attribute.c alan3/checkentry.c alan3/class.c
        alan3/converter.c alan3/current.c alan3/debug.c alan3/decode.c
        alan3/dictionary.c alan3/event.c alan3/exe.c alan3/fnmatch.c
        alan3/glkio.c alan3/glkstart.c alan3/instance.c alan3/inter.c
        alan3/lists.c alan3/literal.c alan3/main.c alan3/memory.c alan3/msg.c
        alan3/options.c alan3/output.c alan3/params.c alan3/parse.c
        alan3/readline.c alan3/reverse.c alan3/rules.c alan3/save.c alan3/scan.c
        alan3/score.c alan3/set.c alan3/stack.c alan3/state.c alan3/syntax.c
        alan3/sysdep.c alan3/syserr.c alan3/term.c alan3/types.c alan3/utils.c
        alan3/word.c alan3/compatibility.c alan3/AltInfo.c alan3/Container.c
        alan3/Location.c alan3/ParameterPosition.c alan3/StateStack.c
        MACROS GLK HAVE_GARGLK BUILD=0
        POSIX)
endif()

# ------------------------------------------------------------------------------
#
# Bocfel 2.2.4
#
if(WITH_BOCFEL)
    set(BOCFEL_MACROS ZTERP_GLK ZTERP_GLK_BLORB ZTERP_GLK_UNIX "ZTERP_STATIC_PATCH_FILE=\"static-patches.h\"")
    if(UNIX)
        list(APPEND BOCFEL_MACROS ZTERP_OS_UNIX)
    elseif(MINGW)
        list(APPEND BOCFEL_MACROS ZTERP_OS_WIN32)
    endif()

    if(GARGLK_NEEDS_TICK)
        list(APPEND BOCFEL_MACROS ZTERP_GLK_TICK)
    endif()

    terp(bocfel
        SRCS bocfel/blorb.cpp bocfel/branch.cpp bocfel/dict.cpp bocfel/iff.cpp
        bocfel/io.cpp bocfel/mathop.cpp bocfel/meta.cpp bocfel/memory.cpp bocfel/objects.cpp
        bocfel/options.cpp bocfel/osdep.cpp bocfel/patches.cpp bocfel/process.cpp
        bocfel/random.cpp bocfel/screen.cpp bocfel/sound.cpp bocfel/stack.cpp
        bocfel/stash.cpp bocfel/unicode.cpp bocfel/util.cpp bocfel/zoom.cpp bocfel/zterp.cpp
        bocfel/glkstart.cpp
        CXXSTD 14
        MACROS ${BOCFEL_MACROS}
        LTO WARNINGS)

    if(CMAKE_C_COMPILER_ID MATCHES "GNU$")
        target_compile_options(bocfel PRIVATE "-Wno-sign-compare")
    endif()
endif()

# ------------------------------------------------------------------------------
#
# Git 1.3.8
#
# Reworked startup code to redirect errors to Glk windows.
#
if(WITH_GIT)
    set(GIT_MACROS USE_INLINE)

    if(GARGLK_NEEDS_TICK)
        list(APPEND GIT_MACROS GIT_NEED_TICK)
    endif()

    if(GARGLK_BIG_ENDIAN)
        list(APPEND GIT_MACROS USE_BIG_ENDIAN)
    endif()

    terp(git
        SRCS git/git.c git/memory.c git/compiler.c git/opcodes.c git/operands.c
        git/peephole.c git/terp.c git/glkop.c git/search.c git/git_unix.c
        git/savefile.c git/saveundo.c git/gestalt.c git/heap.c git/accel.c
        MACROS ${GIT_MACROS}
        MATH)
endif()

# ------------------------------------------------------------------------------
#
# Glulxe 0.6.1
#
if(WITH_GLULXE)
    set(GLULXE_MACROS FLOAT_COMPILE_SAFER_POWF)
    if(APPLE)
        list(APPEND GLULXE_MACROS OS_MAC)
        # _DARWIN_C_SOURCE for arc4random().
        set_source_files_properties(glulxe/osdepend.c PROPERTIES COMPILE_DEFINITIONS _DARWIN_C_SOURCE)
    elseif(UNIX)
        list(APPEND GLULXE_MACROS OS_UNIX)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            list(APPEND GLULXE_MACROS UNIX_RAND_GETRANDOM)
        endif()
    elseif(MINGW OR MSVC)
        list(APPEND GLULXE_MACROS OS_WINDOWS)
    endif()

    terp(glulxe
        SRCS glulxe/main.c glulxe/files.c glulxe/vm.c glulxe/exec.c
        glulxe/funcs.c glulxe/operand.c glulxe/string.c glulxe/glkop.c
        glulxe/heap.c glulxe/serial.c glulxe/search.c glulxe/gestalt.c
        glulxe/osdepend.c glulxe/unixstrt.c glulxe/accel.c glulxe/profile.c
        glulxe/float.c
        MACROS ${GLULXE_MACROS}
        MATH
        POSIX
        LTO)
endif()

# ------------------------------------------------------------------------------
#
# Hugo for Unix git-df0ce19
#
if(WITH_HUGO)
    terp(hugo
        SRCS hugo/heglk/heglk.c hugo/heglk/heglkunix.c hugo/source/he.c
        hugo/source/hemisc.c hugo/source/heparse.c hugo/source/herun.c
        hugo/source/heexpr.c hugo/source/heobject.c hugo/source/heres.c
        hugo/source/heset.c hugo/source/stringfn.c
        INCLUDE_DIRS hugo/source
        MACROS GLK HUGO_GLKUNIX COMPILE_V25 NO_KEYPRESS_CURSOR "HUGO_INLINE=static inline")
endif()

# ------------------------------------------------------------------------------
#
# JACL git-ce6517f
#
if(WITH_JACL)
    terp(jacl
        SRCS jacl/jacl.c jacl/glk_startup.c jacl/findroute.c jacl/interpreter.c
        jacl/loader.c jacl/glk_saver.c jacl/logging.c jacl/parser.c
        jacl/display.c jacl/utils.c jacl/jpp.c jacl/resolvers.c jacl/errors.c
        jacl/encapsulate.c jacl/libcsv.c
        MACROS GLK
        MATH
        POSIX
        GPL2)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang$")
        # This is a style choice in the code.
        target_compile_options(jacl PRIVATE "-Wno-parentheses-equality")

        # Non-prototype (K&R) declarations have been fixed in JACL's Git
        # repository, but there hasn't been a release yet since then, so
        # just ignore the warnings here; remove in the future when a new
        # JACL is upstreamed.
        target_compile_options(jacl PRIVATE "-Wno-deprecated-non-prototype")
    endif()
endif()

# ------------------------------------------------------------------------------
#
# Level9 5.1
#
# Disabled the status bar that just contained a static version string.
# Added graphics support.
#
if(WITH_LEVEL9)
    terp(level9
        SRCS level9/Glk/glk.c level9/bitmap.c level9/level9.c
        INCLUDE_DIRS level9
        MACROS PRIVATE BITMAP_DECODER NEED_STRICMP_PROTOTYPE
        stricmp=gln_strcasecmp strnicmp=gln_strncasecmp
        MATH)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang$")
        # This is a style choice in the code.
        target_compile_options(level9 PRIVATE "-Wno-switch")
    endif()
endif()

# ------------------------------------------------------------------------------
#
# Magnetic 2.3.1
#
# Disable layered drawing, because that is slower than drawing
# all the pixels with fill_rect -- the opposite of Xglk.
#
# Delay opening the status window, because for games that don't use it
# magnetic shows a static version string only. I don't like that.
#
# Made shaded border around pictures if-defable.
#
if(WITH_MAGNETIC)
    terp(magnetic
        SRCS magnetic/Generic/emu.c magnetic/Glk/glk.c
        INCLUDE_DIRS magnetic/Generic
        MACROS MAGNETIC_GLKUNIX)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang$")
        # The alternative to this is lots of casts; ignore this warning
        # unless it becomes a problem with a more strict compiler.
        target_compile_options(magnetic PRIVATE "-Wno-pointer-sign")
    endif()
endif()

# ------------------------------------------------------------------------------
#
# Plus 1.0 for Scott Adams Graphic Adventures Plus games
#
if(WITH_PLUS)
    set(PLUS_INCLUDE_DIRS plus)
    terp(plus
        SRCS plus/plusmain.c plus/animations.c plus/apple2detect.c plus/apple2draw.c
        plus/atari8c64draw.c plus/atari8detect.c plus/c64detect.c plus/extracommands.c
        plus/companionfile.c plus/gameinfo.c plus/graphics.c plus/layouttext.c plus/loaddatabase.c
        plus/parseinput.c plus/pcdraw.c plus/restorestate.c plus/stdetect.c plus/stdraw.c
        INCLUDE_DIRS ${PLUS_INCLUDE_DIRS}
        LIBS c64diskimage
        GPL2)
endif()

# ------------------------------------------------------------------------------
#
# SCARE 1.3.10 for Adrift 4 games
#
if(WITH_SCARE)
    if(UNIX OR MINGW OR MSVC)
        find_package(ZLIB REQUIRED)
        set(SCARE_INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS})
        set(SCARE_LIBS ${ZLIB_LIBRARIES})
    elseif(MINGW)
        set(SCARE_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/support/zlib)
        set(SCARE_LIBS ${PROJECT_SOURCE_DIR}/support/zlib/zlib1.dll)
    else()
        message(FATAL_ERROR "Unknown platform: cannot build scare because zlib is required")
    endif()

    terp(scare
        SRCS scare/sctafpar.c scare/sctaffil.c scare/scprops.c scare/scvars.c
        scare/scexpr.c scare/scprintf.c scare/scinterf.c scare/scparser.c
        scare/sclibrar.c scare/scrunner.c scare/scevents.c scare/scnpcs.c
        scare/scobjcts.c scare/sctasks.c scare/screstrs.c scare/scgamest.c
        scare/scserial.c scare/scresour.c scare/scutils.c scare/scdebug.c
        scare/sclocale.c scare/scmemos.c scare/os_glk.c
        INCLUDE_DIRS ${SCARE_INCLUDE_DIRS}
        LIBS ${SCARE_LIBS}
        MACROS SCARE_NO_ABBREVIATIONS
        GPL2)
endif()

# ------------------------------------------------------------------------------
#
# ScottFree 1.14 for Scott Adams games
#

if(WITH_SCOTT)
    set(SCOTT_INCLUDE_DIRS scott scott/saga scott/ai_uk scott/ti994a)
    terp(scott
        SRCS scott/scott.c scott/bsd.c scott/ai_uk/c64decrunch.c scott/ai_uk/debug.c
        scott/ai_uk/decompresstext.c scott/ai_uk/line_drawing.c
        scott/ai_uk/game_specific.c scott/ai_uk/hulk.c scott/ai_uk/gremlins.c
        scott/ai_uk/decompressz80.c scott/detectgame.c scott/scottgameinfo.c scott/layouttext.c
        scott/ti994a/load_ti99_4a.c scott/parser.c scott/restorestate.c
        scott/ai_uk/ringbuffer.c scott/ai_uk/robinofsherwood.c scott/ai_uk/sagadraw.c
        scott/saga/apple2detect.c scott/saga/apple2draw.c scott/saga/atari8c64draw.c
        scott/saga/atari8detect.c scott/saga/ciderpress.c scott/saga/pcdraw.c scott/saga/saga.c
        scott/saga/sagagraphics.c scott/saga/woz2nib.c scott/titleimage.c
        scott/ai_uk/seasofblood.c scott/ti994a/ti99_4a_terp.c
        INCLUDE_DIRS ${SCOTT_INCLUDE_DIRS}
        LIBS c64diskimage unp64)
endif()

# ------------------------------------------------------------------------------
#
# TADS
#
if(WITH_TADS)
    set(srcs tads/glk/memicmp.c tads/glk/osbuffer.c tads/glk/osextra.c
        tads/glk/osglk.c tads/glk/osglkban.c tads/glk/osmisc.c
        tads/glk/osparse.c tads/glk/osportable.cc tads/glk/t23run.cpp
        tads/glk/t2askf.c tads/glk/t2indlg.c tads/glk/t3askf.cpp
        tads/glk/t3indlg.cpp tads/glk/vmuni_cs.cpp)

    list(APPEND srcs tads/tads2/argize.c tads/tads2/bif.c tads/tads2/bifgdum.c
        tads/tads2/cmap.c tads/tads2/cmd.c tads/tads2/dat.c tads/tads2/dbgtr.c
        tads/tads2/errmsg.c tads/tads2/execmd.c tads/tads2/fio.c
        tads/tads2/fioxor.c tads/tads2/getstr.c tads/tads2/ler.c
        tads/tads2/linfdum.c tads/tads2/lst.c tads/tads2/mch.c tads/tads2/mcm.c
        tads/tads2/mcs.c tads/tads2/obj.c tads/tads2/oem.c tads/tads2/os0.c
        tads/tads2/oserr.c tads/tads2/osifc.c tads/tads2/osnoui.c
        tads/tads2/osrestad.c tads/tads2/osstzprs.c tads/tads2/ostzposix.c
        tads/tads2/out.c tads/tads2/output.c tads/tads2/ply.c tads/tads2/qas.c
        tads/tads2/regex.c tads/tads2/run.c tads/tads2/runstat.c
        tads/tads2/suprun.c tads/tads2/trd.c tads/tads2/voc.c
        tads/tads2/vocab.c)

    list(APPEND srcs tads/tads3/charmap.cpp tads/tads3/md5.cpp
        tads/tads3/resldexe.cpp tads/tads3/resload.cpp tads/tads3/sha2.cpp
        tads/tads3/std.cpp tads/tads3/tcerr.cpp tads/tads3/tcerrmsg.cpp
        tads/tads3/tcgen.cpp tads/tads3/tcglob.cpp tads/tads3/tcmain.cpp
        tads/tads3/tcprs.cpp tads/tads3/tcprs_rt.cpp tads/tads3/tcprsnf.cpp
        tads/tads3/tcprsnl.cpp tads/tads3/tcprsstm.cpp tads/tads3/tcsrc.cpp
        tads/tads3/tct3.cpp tads/tads3/tct3_d.cpp tads/tads3/tct3nl.cpp
        tads/tads3/tct3stm.cpp tads/tads3/tct3unas.cpp tads/tads3/tctok.cpp
        tads/tads3/utf8.cpp tads/tads3/vmanonfn.cpp tads/tads3/vmbif.cpp
        tads/tads3/vmbifl.cpp tads/tads3/vmbifreg.cpp tads/tads3/vmbift3.cpp
        tads/tads3/vmbiftad.cpp tads/tads3/vmbiftio.cpp tads/tads3/vmbignum.cpp
        tads/tads3/vmbignumlib.cpp tads/tads3/vmbt3_nd.cpp
        tads/tads3/vmbytarr.cpp tads/tads3/vmcfgmem.cpp tads/tads3/vmcoll.cpp
        tads/tads3/vmconhmp.cpp tads/tads3/vmconsol.cpp tads/tads3/vmcrc.cpp
        tads/tads3/vmcset.cpp tads/tads3/vmdate.cpp tads/tads3/vmdict.cpp
        tads/tads3/vmdynfunc.cpp tads/tads3/vmerr.cpp tads/tads3/vmerrmsg.cpp
        tads/tads3/vmfile.cpp tads/tads3/vmfilnam.cpp tads/tads3/vmfilobj.cpp
        tads/tads3/vmfref.cpp tads/tads3/vmfunc.cpp tads/tads3/vmglob.cpp
        tads/tads3/vmgram.cpp tads/tads3/vmhash.cpp tads/tads3/vmhostsi.cpp
        tads/tads3/vmhosttx.cpp tads/tads3/vmimage.cpp tads/tads3/vmimg_nd.cpp
        tads/tads3/vmini_nd.cpp tads/tads3/vminit.cpp tads/tads3/vminitim.cpp
        tads/tads3/vmintcls.cpp tads/tads3/vmisaac.cpp tads/tads3/vmiter.cpp
        tads/tads3/vmlog.cpp tads/tads3/vmlookup.cpp tads/tads3/vmlst.cpp
        tads/tads3/vmmain.cpp tads/tads3/vmmcreg.cpp tads/tads3/vmmeta.cpp
        tads/tads3/vmnetfillcl.cpp tads/tads3/vmobj.cpp tads/tads3/vmop.cpp
        tads/tads3/vmpack.cpp tads/tads3/vmpat.cpp tads/tads3/vmpool.cpp
        tads/tads3/vmpoolim.cpp tads/tads3/vmregex.cpp tads/tads3/vmrun.cpp
        tads/tads3/vmrunsym.cpp tads/tads3/vmsa.cpp tads/tads3/vmsave.cpp
        tads/tads3/vmsort.cpp tads/tads3/vmsortv.cpp tads/tads3/vmsrcf.cpp
        tads/tads3/vmstack.cpp tads/tads3/vmstr.cpp tads/tads3/vmstrbuf.cpp
        tads/tads3/vmstrcmp.cpp tads/tads3/vmtmpfil.cpp tads/tads3/vmtobj.cpp
        tads/tads3/vmtype.cpp tads/tads3/vmtypedh.cpp tads/tads3/vmtz.cpp
        tads/tads3/vmtzobj.cpp tads/tads3/vmundo.cpp tads/tads3/vmvec.cpp)

    list(APPEND srcs tads/tads3/vmconnom.cpp)

    set(macros GLK VMGLOB_STRUCT GLK_UNICODE GLK_TIMERS TC_TARGET_T3 RUNTIME)
    if(MINGW32 OR MSVC)
        list(APPEND macros DMSDOS DT_WIN32)
    endif()

    terp(tadsr
        SRCS ${srcs}
        MACROS ${macros}
        CXXSTD 11
        POSIX
        GPL2)

    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
        # This is a style choice in the code.
        target_compile_options(tadsr PRIVATE "-Wno-logical-not-parentheses")
    endif()

    target_include_directories(tadsr PRIVATE tads/glk tads/tads2 tads/tads3)
endif()

# ------------------------------------------------------------------------------
#
# TaylorMade 0.4 for later Adventure Soft UK games
#

if(WITH_TAYLOR)
    set(TAYLOR_INCLUDE_DIRS taylor)
    terp(taylor
        SRCS taylor/animations.c taylor/c64decrunch.c taylor/decompressz80.c
        taylor/decrypttotloader.c taylor/extracommands.c taylor/extracttape.c taylor/gameinfo.c
        taylor/layouttext.c taylor/loadtotpicture.c taylor/parseinput.c taylor/player.c
        taylor/restorestate.c taylor/sagadraw.c taylor/ui.c taylor/utility.c
        INCLUDE_DIRS ${TAYLOR_INCLUDE_DIRS}
        LIBS c64diskimage unp64
        GPL2)
endif()

if(WITH_FRANKENDRIFT)
    add_subdirectory(frankendrift/FrankenDrift.GlkRunner/FrankenDrift.GlkRunner.Gargoyle)
endif()
