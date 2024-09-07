cmake_minimum_required(VERSION 3.13)

project(TADS)

# Add sources to a target from a specified directory
function(add_sources target dir)
    cmake_parse_arguments(PARSE_ARGV 2 ADD "" "" "SRCS")
    list(TRANSFORM ADD_SRCS PREPEND ${dir})
    target_sources(${target} PRIVATE ${ADD_SRCS})
endfunction()

# Find the requested library
set(GlkLibPath "glk" CACHE PATH "Glk Library Path")
set(GlkLibName "" CACHE STRING "Glk Library Name (without lib- or -.a)")
if (GlkLibName STREQUAL "")
    get_filename_component(GlkLibNameReal ${GlkLibPath} NAME)
else ()
    set(GlkLibNameReal ${GlkLibName} STRING)
endif()
add_library(glk STATIC IMPORTED)
set_target_properties(glk PROPERTIES IMPORTED_LOCATION "${GlkLibPath}/lib${GlkLibNameReal}.a")
target_include_directories(glk INTERFACE ${GlkLibPath})

add_executable(tadsr)
target_link_libraries(tadsr glk)
target_include_directories(tadsr PRIVATE . ../tads2/ ../tads3/)
add_sources(tadsr "./"
    SRCS memicmp.c osbuffer.c osextra.c osglk.c osglkban.c osmisc.c osparse.c
    osportable.cc t23run.cpp t2askf.c t2indlg.c t3askf.cpp t3indlg.cpp vmuni_cs.cpp)
add_sources(tadsr "../tads2/"
    SRCS argize.c bif.c bifgdum.c cmap.c cmd.c dat.c dbgtr.c errmsg.c execmd.c
    fio.c fioxor.c getstr.c ler.c linfdum.c lst.c mch.c mcm.c mcs.c obj.c oem.c
    os0.c oserr.c osifc.c osnoui.c osrestad.c osstzprs.c ostzposix.c out.c output.c
    ply.c qas.c regex.c run.c runstat.c suprun.c trd.c voc.c vocab.c)
add_sources(tadsr "../tads3/"
    SRCS charmap.cpp md5.cpp resldexe.cpp resload.cpp sha2.cpp std.cpp tcerr.cpp
    tcerrmsg.cpp tcgen.cpp tcglob.cpp tcmain.cpp tcprs.cpp tcprs_rt.cpp tcprsnf.cpp
    tcprsnl.cpp tcprsstm.cpp tcsrc.cpp tct3.cpp tct3_d.cpp tct3nl.cpp tct3stm.cpp
    tct3unas.cpp tctok.cpp utf8.cpp vmanonfn.cpp vmbif.cpp vmbifl.cpp vmbifreg.cpp
    vmbift3.cpp vmbiftad.cpp vmbiftio.cpp vmbignum.cpp vmbignumlib.cpp vmbt3_nd.cpp
    vmbytarr.cpp vmcfgmem.cpp vmcoll.cpp vmconhmp.cpp vmconsol.cpp vmcrc.cpp
    vmcset.cpp vmdate.cpp vmdict.cpp vmdynfunc.cpp vmerr.cpp vmerrmsg.cpp
    vmfile.cpp vmfilnam.cpp vmfilobj.cpp vmfref.cpp vmfunc.cpp vmglob.cpp
    vmgram.cpp vmhash.cpp vmhostsi.cpp vmhosttx.cpp vmimage.cpp vmimg_nd.cpp
    vmini_nd.cpp vminit.cpp vminitim.cpp vmintcls.cpp vmisaac.cpp vmiter.cpp
    vmlog.cpp vmlookup.cpp vmlst.cpp vmmain.cpp vmmcreg.cpp vmmeta.cpp
    vmnetfillcl.cpp vmobj.cpp vmop.cpp vmpack.cpp vmpat.cpp vmpool.cpp
    vmpoolim.cpp vmregex.cpp vmrun.cpp vmrunsym.cpp vmsa.cpp vmsave.cpp vmsort.cpp
    vmsortv.cpp vmsrcf.cpp vmstack.cpp vmstr.cpp vmstrbuf.cpp vmstrcmp.cpp
    vmtmpfil.cpp vmtobj.cpp vmtype.cpp vmtypedh.cpp vmtz.cpp vmtzobj.cpp
    vmundo.cpp vmvec.cpp vmconnom.cpp)
set_property(TARGET tadsr PROPERTY CXX_STANDARD 11)
target_compile_definitions(tadsr PRIVATE GLK GLK_TIMERS GLK_UNICODE TC_TARGET_T3 RUNTIME VMGLOB_STRUCT)
if(MINGW32)
    target_compile_definitions(tadsr PRIVATE DMSDOS DT_WIN32)
endif()
if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    # This is a style choice in the code.
    target_compile_options(tadsr PRIVATE -Wno-deprecated-register -Wno-logical-not-parentheses -Wno-pointer-sign -Wno-string-concatenation)
endif()