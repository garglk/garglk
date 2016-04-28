function(interpreter target)
  set(options C99 LTO)
  set(oneValueArgs)
  set(multiValueArgs SRCS INCLUDE_DIRS MACROS LIBS)
  cmake_parse_arguments(TERP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  add_executable(${target} ${TERP_SRCS})
  set_target_properties(${target} PROPERTIES OUTPUT_NAME "${GARGLKPRE}${target}")
  target_compile_definitions(${target} PRIVATE GARGLK ${TERP_MACROS})
  target_include_directories(${target} PRIVATE ../garglk ${TERP_INCLUDE_DIRS})
  target_link_libraries(${target} glkmain garglk ${TERP_LIBS})
  if(TERP_C99)
    c99(${target})
  endif()
  if(TERP_LTO)
    lto(${target})
  endif()
  install(TARGETS ${target} DESTINATION libexec/gargoyle)
endfunction()

# -----------------------------------------------------------------------
#
# AdvSys 1.2 + ANSI + NewParser + parts of glkize
#
interpreter(advsys
            SRCS advsys/advmsg.c advsys/advtrm.c advsys/advprs.c advsys/advdbs.c
            advsys/advint.c advsys/advjunk.c advsys/advexe.c advsys/glkstart.c)

# -----------------------------------------------------------------------
#
# Agility 1.1.1.1 with Glk
#
# Agility uses some bad voodoo to get filenames back from glk filerefs.
# We have three options:
#   1) use the bad voodoo (only works on unixes)
#   2) don't use glk file prompts (GLK_ANSI_ONLY)
#   3) use garglk extension garglk_fileref_get_name (GARGLK)
#
interpreter(agility
            SRCS agility/agtread.c agility/gamedata.c agility/util.c
            agility/agxfile.c agility/auxfile.c agility/filename.c
            agility/parser.c agility/exec.c agility/runverb.c
            agility/metacommand.c agility/savegame.c agility/debugcmd.c
            agility/agil.c agility/token.c agility/disassemble.c
            agility/object.c agility/interface.c agility/os_glk.c
            MACROS GLK GLK_ANSI_ONLY)

# -----------------------------------------------------------------------
#
# Alan interpreter 2.8.6
#
# Fixed a couple of bugs (needsp needs resetting in a few places).
# Added glk file prompts for gargoyle using garglk_fileref_get_name.
#
interpreter(alan2
            SRCS alan2/arun.c alan2/main.c alan2/debug.c alan2/args.c
            alan2/exe.c alan2/inter.c alan2/parse.c alan2/rules.c alan2/stack.c
            alan2/decode.c alan2/term.c alan2/reverse.c alan2/readline.c
            alan2/params.c alan2/sysdep.c alan2/glkstart.c alan2/glkio.c
            alan2/alan.version.c
            MACROS GLK RESERVED)

# -----------------------------------------------------------------------
#
# Alan interpreter 3 beta 2
#
interpreter(alan3
            SRCS alan3/alan.version.c alan3/act.c alan3/actor.c alan3/args.c
            alan3/arun.c alan3/attribute.c alan3/checkentry.c alan3/class.c
            alan3/current.c alan3/debug.c alan3/decode.c alan3/dictionary.c
            alan3/event.c alan3/exe.c alan3/glkio.c alan3/glkstart.c
            alan3/instance.c alan3/inter.c alan3/lists.c alan3/literal.c
            alan3/main.c alan3/memory.c alan3/msg.c alan3/options.c
            alan3/output.c alan3/params.c alan3/parse.c alan3/readline.c
            alan3/reverse.c alan3/rules.c alan3/save.c alan3/scan.c
            alan3/score.c alan3/set.c alan3/stack.c alan3/state.c alan3/syntax.c
            alan3/sysdep.c alan3/syserr.c alan3/term.c alan3/utils.c
            alan3/word.c alan3/compatibility.c alan3/AltInfo.c alan3/Container.c
            alan3/Location.c alan3/ParameterPosition.c alan3/StateStack.c
            MACROS GLK HAVE_GARGLK BUILD=0)

# -----------------------------------------------------------------------
#
# Bocfel 0.6.1
#
set(BOCFEL_MACROS ZTERP_GLK)
if(UNIX)
  list(APPEND BOCFEL_MACROS ZTERP_UNIX)
elseif(WIN32)
  list(APPEND BOCFEL_MACROS ZTERP_WIN32)
endif()

interpreter(bocfel
            SRCS bocfel/blorb.c bocfel/branch.c bocfel/dict.c bocfel/iff.c
            bocfel/io.c bocfel/math.c bocfel/meta.c bocfel/memory.c
            bocfel/objects.c bocfel/osdep.c bocfel/process.c bocfel/random.c
            bocfel/screen.c bocfel/sound.c bocfel/stack.c bocfel/unicode.c
            bocfel/util.c bocfel/zoom.c bocfel/zterp.c bocfel/glkstart.c
            MACROS ${BOCFEL_MACROS}
            C99 LTO)

# -----------------------------------------------------------------------
#
# Frotz 2.50 / glk port
#
interpreter(frotz
            SRCS frotz/buffer.c frotz/err.c frotz/fastmem.c frotz/files.c
            frotz/input.c frotz/main.c frotz/math.c frotz/object.c
            frotz/process.c frotz/quetzal.c frotz/random.c frotz/redirect.c
            frotz/sound.c frotz/stream.c frotz/table.c frotz/text.c
            frotz/variable.c frotz/glkscreen.c frotz/glkmisc.c)

# -----------------------------------------------------------------------
#
# Geas 0.4
#
interpreter(geas
            SRCS geas/geasfile.cc geas/geas-runner.cc geas/geas-state.cc
            geas/geas-util.cc geas/geasglk.cc geas/geasglkterm.c geas/istring.cc
            geas/readfile.cc
            MACROS USE_INLINE)

# -----------------------------------------------------------------------
#
# Git 1.3.3
#
# Reworked startup code to redirect errors to Glk windows.
#
interpreter(git
            SRCS git/git.c git/memory.c git/compiler.c git/opcodes.c
            git/operands.c git/peephole.c git/terp.c git/glkop.c git/search.c
            git/git_unix.c git/savefile.c git/saveundo.c git/gestalt.c
            git/heap.c git/accel.c
            MACROS USE_INLINE USE_OWN_POWF
            LIBS m)

# -----------------------------------------------------------------------
#
# Glulxe 0.5.2
#
set(GLULXE_MACROS FLOAT_COMPILE_SAFER_POWF)
if(UNIX)
  list(APPEND GLULXE_MACROS OS_UNIX)
endif()
interpreter(glulxe
            SRCS glulxe/main.c glulxe/files.c glulxe/vm.c glulxe/exec.c
            glulxe/funcs.c glulxe/operand.c glulxe/string.c glulxe/glkop.c
            glulxe/heap.c glulxe/serial.c glulxe/search.c glulxe/gestalt.c
            glulxe/osdepend.c glulxe/unixstrt.c glulxe/accel.c glulxe/profile.c
            glulxe/float.c
            MACROS ${GLULXE_MACROS}
            LIBS m)

# -----------------------------------------------------------------------
#
# Hugo 3.1.03
#
# Include hemedia.c from heglk.c for sound/graphics support.
# Added set_base_file so it can find resource files.
#
interpreter(hugo
            SRCS hugo/heglk/heglk.c hugo/heglk/heglkunix.c hugo/source/he.c
            hugo/source/hemisc.c hugo/source/heparse.c hugo/source/herun.c
            hugo/source/heexpr.c hugo/source/heobject.c hugo/source/heres.c
            hugo/source/heset.c hugo/source/stringfn.c
            INCLUDE_DIRS hugo/source
            MACROS GLK COMPILE_V25 NO_KEYPRESS_CURSOR "HUGO_INLINE=static inline")

# -----------------------------------------------------------------------
#
# JACL 2.9.0 for JACL games
#
interpreter(jacl
            SRCS jacl/jacl.c jacl/glk_startup.c jacl/findroute.c
            jacl/interpreter.c jacl/loader.c jacl/glk_saver.c jacl/logging.c
            jacl/parser.c jacl/display.c jacl/utils.c jacl/jpp.c
            jacl/resolvers.c jacl/errors.c jacl/encapsulate.c jacl/libcsv.c
            MACROS GLK
            LIBS m)

# -----------------------------------------------------------------------
#
# Level9 4.1
#
# Disabled the status bar that just contained a static version string.
# Added graphics support.
#
interpreter(level9
            SRCS level9/Glk/glk.c level9/bitmap.c level9/level9.c
            INCLUDE_DIRS level9
            MACROS PRIVATE BITMAP_DECODER NEED_STRICMP_PROTOTYPE
            stricmp=gln_strcasecmp strnicmp=gln_strncasecmp
            LIBS m)

# -----------------------------------------------------------------------
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
interpreter(magnetic
            SRCS magnetic/Generic/emu.c magnetic/Glk/glk.c
            INCLUDE_DIRS magnetic/Generic)

# -----------------------------------------------------------------------
#
# Nitfol 0.5 + lots of patches
#
# Define the stuff you want:
#
# DEBUGGING   Compile in the debugger.  See DEBUG for details
# SMART_TOKENISER Tries to correct typos.
# TWOS16SHORT Try to speed up some operations if 'short' is a 16 bit two's
#             complement number on your system.  Note that this might slow down
#             Pentium IIs.
# FAST_SIGNED If you're on something which does signed math much faster than
#             unsigned, define this
# FAST        Normally, nitfol does lots of tests to make sure values are in
#             range, both to alert games of unportable actions and to prevent
#             nitfol from crashing.  Defining this removes those tests, so will
#             speed up the game a bit.  Completely legal games should run
#             exactly the same with or without this flag, so if you have a
#             slow computer and you know all the games you will be running are
#             completely legal, define this.
# USE_INLINE  If your C compiler uses the keyword 'inline' to inline functions
# NO_LIBC     Cause nitfol to use its own functions rather than your libc
# NO_TICK     If you don't need glk_tick, define this (minor speed improvement)
# _GNU_SOURCE Use GNU functions (currently just canonicalize_file_name)
# _BSD_SOURCE Use BSD functions (just realpath)
# _XOPEN_SOURCE (also lets nitfol use realpath)
interpreter(nitfol
            SRCS nitfol/automap.c nitfol/solve.c nitfol/infix.c nitfol/copying.c
            nitfol/debug.c nitfol/inform.c nitfol/quetzal.c nitfol/undo.c
            nitfol/op_call.c nitfol/decode.c nitfol/errmesg.c nitfol/globals.c
            nitfol/iff.c nitfol/init.c nitfol/io.c nitfol/z_io.c nitfol/op_jmp.c
            nitfol/op_math.c nitfol/op_save.c nitfol/op_table.c nitfol/op_v6.c
            nitfol/oplist.c nitfol/stack.c nitfol/zscii.c nitfol/tokenise.c
            nitfol/struct.c nitfol/objects.c nitfol/portfunc.c nitfol/hash.c
            nitfol/sound.c nitfol/graphics.c nitfol/blorb.c nitfol/main.c
            nitfol/startunix.c
            MACROS SMART_TOKENISER DEBUGGING FAST USE_INLINE NO_TICK)

# -----------------------------------------------------------------------
#
# SCARE 1.3.10 for Adrift games
#
if(UNIX)
  find_package(ZLIB)
  if(ZLIB_FOUND)
    set(SCARE_INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS})
    set(SCARE_LIBS ${ZLIB_LIBRARIES})
  else()
    message(WARNING "Unable to find zlib: scare will not be built")
  endif()
elseif(MINGW)
  set(ZLIB_FOUND TRUE)
  set(SCARE_INCLUDE_DIRS ../support/zlib)
  set(SCARE_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../support/zlib/zlib1.dll)
endif()

if(ZLIB_FOUND)
  interpreter(scare
              SRCS scare/sctafpar.c scare/sctaffil.c scare/scprops.c
              scare/scvars.c scare/scexpr.c scare/scprintf.c scare/scinterf.c
              scare/scparser.c scare/sclibrar.c scare/scrunner.c
              scare/scevents.c scare/scnpcs.c scare/scobjcts.c scare/sctasks.c
              scare/screstrs.c scare/scgamest.c scare/scserial.c
              scare/scresour.c scare/scutils.c scare/scdebug.c scare/sclocale.c
              scare/scmemos.c scare/os_glk.c
              INCLUDE_DIRS ${SCARE_INCLUDE_DIRS}
              LIBS ${SCARE_LIBS})
endif()

# -----------------------------------------------------------------------
#
# ScottFree 1.14 for Scott Adams games
#
interpreter(scott
            SRCS scott/scott.c
            C99)
