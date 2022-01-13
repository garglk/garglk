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
option(WITH_SCARE "Enable the scare interpreter" ON)
option(WITH_SCOTT "Enable the scott interpreter" ON)

function(terp target)
    set(options POSIX LTO MATH WARNINGS)
    set(oneval CXXSTD)
    set(multival SRCS INCLUDE_DIRS MACROS LIBS)
    cmake_parse_arguments(TERP "${options}" "${oneval}" "${multival}" ${ARGN})

    add_executable(${target} WIN32 ${TERP_SRCS})
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "${GARGLKPRE}${target}")
    target_compile_definitions(${target} PRIVATE GARGLK ${TERP_MACROS})
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/garglk" ${TERP_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE garglkmain garglk ${TERP_LIBS})

    if(MINGW)
        target_sources(${target} PRIVATE "${PROJECT_SOURCE_DIR}/garglk/icons.rc")
    endif()

    if(NOT TERP_CXXSTD)
        set(TERP_CXXSTD 98)
    endif()

    c_standard(${target} 11)
    cxx_standard(${target} ${TERP_CXXSTD})

    if(TERP_POSIX)
        target_compile_definitions(${target} PRIVATE "_XOPEN_SOURCE=600")
        target_compile_definitions(${target} PRIVATE "_POSIX_C_SOURCE=200112L")
    endif()
    if(TERP_LTO)
        lto(${target})
    endif()
    if(TERP_MATH)
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

add_subdirectory(tads)

# ------------------------------------------------------------------------------
#
# AdvSys 1.2 + ANSI + NewParser + parts of glkize
#
if(WITH_ADVSYS)
    terp(advsys
        SRCS advsys/advmsg.c advsys/advtrm.c advsys/advprs.c
        advsys/advdbs.c advsys/advint.c advsys/advjunk.c advsys/advexe.c
        advsys/glkstart.c)

    if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang$")
        # This is a style choice in the code.
        target_compile_options(advsys PRIVATE "-Wno-parentheses")
    endif()
endif()

# ------------------------------------------------------------------------------
#
# Agility 1.1.1.1 with Glk
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
        MACROS GLK GLK_ANSI_ONLY POSIX)
endif()

# ------------------------------------------------------------------------------
#
# Alan interpreter 2.8.6
#
# Fixed a couple of bugs (needsp needs resetting in a few places).
# Added glk file prompts for gargoyle using garglk_fileref_get_name.
#
if(WITH_ALAN2)
    terp(alan2
        SRCS alan2/arun.c alan2/main.c alan2/debug.c alan2/args.c alan2/exe.c
        alan2/inter.c alan2/parse.c alan2/rules.c alan2/stack.c alan2/decode.c
        alan2/term.c alan2/reverse.c alan2/readline.c alan2/params.c
        alan2/sysdep.c alan2/glkstart.c alan2/glkio.c alan2/alan.version.c
        MACROS GLK REVERSED
        POSIX)

    if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang$")
        # This is a style choice in the code.
        target_compile_options(alan2 PRIVATE "-Wno-dangling-else")
    endif()
endif()

# ------------------------------------------------------------------------------
#
# Alan interpreter 3.0beta7
#
if(WITH_ALAN3)
    terp(alan3
        SRCS alan3/alan.version.c alan3/act.c alan3/actor.c alan3/args.c
        alan3/arun.c alan3/attribute.c alan3/checkentry.c alan3/class.c
        alan3/current.c alan3/debug.c alan3/decode.c alan3/dictionary.c
        alan3/event.c alan3/exe.c alan3/fnmatch.c alan3/glkio.c alan3/glkstart.c
        alan3/instance.c alan3/inter.c alan3/lists.c alan3/literal.c alan3/main.c
        alan3/memory.c alan3/msg.c alan3/options.c alan3/output.c alan3/params.c
        alan3/parse.c alan3/readline.c alan3/reverse.c alan3/rules.c alan3/save.c
        alan3/scan.c alan3/score.c alan3/set.c alan3/stack.c alan3/state.c
        alan3/syntax.c alan3/sysdep.c alan3/syserr.c alan3/term.c alan3/types.c
        alan3/utils.c alan3/word.c alan3/compatibility.c alan3/AltInfo.c
        alan3/Container.c alan3/Location.c alan3/ParameterPosition.c
        alan3/StateStack.c
        MACROS GLK HAVE_GARGLK BUILD=0
        POSIX)
endif()

# ------------------------------------------------------------------------------
#
# Bocfel 1.4
#
set(BOCFEL_MACROS ZTERP_GLK ZTERP_GLK_BLORB)
if(UNIX)
    list(APPEND BOCFEL_MACROS ZTERP_UNIX)
elseif(MINGW)
    list(APPEND BOCFEL_MACROS ZTERP_WIN32)
endif()

if(GARGLK_NEEDS_TICK)
    list(APPEND BOCFEL_MACROS ZTERP_GLK_TICK)
endif()

if(WITH_BOCFEL)
    terp(bocfel
        SRCS bocfel/blorb.c bocfel/branch.c bocfel/dict.c bocfel/iff.c
        bocfel/io.c bocfel/math.c bocfel/meta.c bocfel/memory.c bocfel/objects.c
        bocfel/osdep.c bocfel/patches.c bocfel/process.c bocfel/random.c
        bocfel/screen.c bocfel/sound.c bocfel/stack.c bocfel/stash.c
        bocfel/unicode.c bocfel/util.c bocfel/zoom.c bocfel/zterp.c
        bocfel/glkstart.c
        MACROS ${BOCFEL_MACROS}
        LTO WARNINGS)

    if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang$")
        # The constants this complains about vary in value depending on
        # platform, so although the warning is correct when it occurs,
        # the checks are valid, and the warning unnecessary.
        target_compile_options(bocfel PRIVATE "-Wno-tautological-constant-out-of-range-compare")
    endif()
endif()

# ------------------------------------------------------------------------------
#
# Git 1.3.3
#
# Reworked startup code to redirect errors to Glk windows.
#
set(GIT_MACROS USE_INLINE)

if(GARGLK_NEEDS_TICK)
    list(APPEND GIT_MACROS GIT_NEED_TICK)
endif()
if(WITH_GIT)
    terp(git
        SRCS git/git.c git/memory.c git/compiler.c git/opcodes.c git/operands.c
        git/peephole.c git/terp.c git/glkop.c git/search.c git/git_unix.c
        git/savefile.c git/saveundo.c git/gestalt.c git/heap.c git/accel.c
        MACROS ${GIT_MACROS}
        MATH)
endif()

# ------------------------------------------------------------------------------
#
# Glulxe 0.5.2
#
set(GLULXE_MACROS FLOAT_COMPILE_SAFER_POWF)
if(UNIX)
    list(APPEND GLULXE_MACROS OS_UNIX)
elseif(MINGW)
    list(APPEND GLULXE_MACROS WIN32)
endif()

if(WITH_GLULXE)
    terp(glulxe
        SRCS glulxe/main.c glulxe/files.c glulxe/vm.c glulxe/exec.c
        glulxe/funcs.c glulxe/operand.c glulxe/string.c glulxe/glkop.c
        glulxe/heap.c glulxe/serial.c glulxe/search.c glulxe/gestalt.c
        glulxe/osdepend.c glulxe/unixstrt.c glulxe/accel.c glulxe/profile.c
        glulxe/float.c
        MACROS ${GLULXE_MACROS}
        MATH
        POSIX)
endif()

# ------------------------------------------------------------------------------
#
# Hugo 3.1.06
#
if(WITH_HUGO)
    terp(hugo
        SRCS hugo/heglk/heglk.c hugo/heglk/heglkunix.c hugo/source/he.c
        hugo/source/hemisc.c hugo/source/heparse.c hugo/source/herun.c
        hugo/source/heexpr.c hugo/source/heobject.c hugo/source/heres.c
        hugo/source/heset.c hugo/source/stringfn.c
        INCLUDE_DIRS hugo/source
        MACROS GLK COMPILE_V25 NO_KEYPRESS_CURSOR "HUGO_INLINE=static inline")
endif()

# ------------------------------------------------------------------------------
#
# JACL 2.9.0 for JACL games
#
if(WITH_JACL)
    terp(jacl
        SRCS jacl/jacl.c jacl/glk_startup.c jacl/findroute.c jacl/interpreter.c
        jacl/loader.c jacl/glk_saver.c jacl/logging.c jacl/parser.c
        jacl/display.c jacl/utils.c jacl/jpp.c jacl/resolvers.c jacl/errors.c
        jacl/encapsulate.c jacl/libcsv.c
        MACROS GLK
        MATH
        POSIX)

    if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang$")
        # This is a style choice in the code.
        target_compile_options(jacl PRIVATE "-Wno-parentheses-equality")
    endif()
endif()

# ------------------------------------------------------------------------------
#
# Level9 4.1
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

    if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang$")
        # This is a style choice in the code.
        target_compile_options(level9 PRIVATE "-Wno-switch")
    endif()
endif()

# ------------------------------------------------------------------------------
#
# Magnetic 2.3
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
        INCLUDE_DIRS magnetic/Generic)
endif()

# ------------------------------------------------------------------------------
#
# SCARE 1.3.10 for Adrift games
#
if(WITH_SCARE)
    if(UNIX OR MINGW)
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
        MACROS SCARE_NO_ABBREVIATIONS)
endif()

# ------------------------------------------------------------------------------
#
# ScottFree 1.14 for Scott Adams games
#
if(WITH_SCOTT)
    terp(scott
        SRCS scott/scott.c scott/bsd.c)
endif()
