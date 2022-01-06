option(WITH_TADS "Enable the tads interpreter" ON)

if(WITH_TADS)
    set(srcs glk/memicmp.c glk/osbuffer.c glk/osextra.c glk/osglk.c
        glk/osglkban.c glk/osmisc.c glk/osparse.c glk/osportable.cc
        glk/t23run.cpp glk/t2askf.c glk/t2indlg.c glk/t3askf.cpp
        glk/t3indlg.cpp glk/vmuni_cs.cpp)

    list(APPEND srcs tads2/argize.c tads2/bif.c tads2/bifgdum.c tads2/cmap.c
        tads2/cmd.c tads2/dat.c tads2/dbgtr.c tads2/errmsg.c tads2/execmd.c
        tads2/fio.c tads2/fioxor.c tads2/getstr.c tads2/ler.c tads2/linfdum.c
        tads2/lst.c tads2/mch.c tads2/mcm.c tads2/mcs.c tads2/obj.c tads2/oem.c
        tads2/os0.c tads2/oserr.c tads2/osifc.c tads2/osnoui.c tads2/osrestad.c
        tads2/osstzprs.c tads2/ostzposix.c tads2/out.c tads2/output.c
        tads2/ply.c tads2/qas.c tads2/regex.c tads2/run.c tads2/runstat.c
        tads2/suprun.c tads2/trd.c tads2/voc.c tads2/vocab.c)

    list(APPEND srcs tads3/charmap.cpp tads3/md5.cpp tads3/resldexe.cpp
        tads3/resload.cpp tads3/sha2.cpp tads3/std.cpp tads3/tcerr.cpp
        tads3/tcerrmsg.cpp tads3/tcgen.cpp tads3/tcglob.cpp tads3/tcmain.cpp
        tads3/tcprs.cpp tads3/tcprs_rt.cpp tads3/tcprsnf.cpp tads3/tcprsnl.cpp
        tads3/tcprsstm.cpp tads3/tcsrc.cpp tads3/tct3.cpp tads3/tct3_d.cpp
        tads3/tct3nl.cpp tads3/tct3stm.cpp tads3/tct3unas.cpp tads3/tctok.cpp
        tads3/utf8.cpp tads3/vmanonfn.cpp tads3/vmbif.cpp tads3/vmbifl.cpp
        tads3/vmbifreg.cpp tads3/vmbift3.cpp tads3/vmbiftad.cpp
        tads3/vmbiftio.cpp tads3/vmbignum.cpp tads3/vmbignumlib.cpp
        tads3/vmbt3_nd.cpp tads3/vmbytarr.cpp tads3/vmcfgmem.cpp
        tads3/vmcoll.cpp tads3/vmconhmp.cpp tads3/vmconsol.cpp tads3/vmcrc.cpp
        tads3/vmcset.cpp tads3/vmdate.cpp tads3/vmdict.cpp tads3/vmdynfunc.cpp
        tads3/vmerr.cpp tads3/vmerrmsg.cpp tads3/vmfile.cpp tads3/vmfilnam.cpp
        tads3/vmfilobj.cpp tads3/vmfref.cpp tads3/vmfunc.cpp tads3/vmglob.cpp
        tads3/vmgram.cpp tads3/vmhash.cpp tads3/vmhostsi.cpp tads3/vmhosttx.cpp
        tads3/vmimage.cpp tads3/vmimg_nd.cpp tads3/vmini_nd.cpp tads3/vminit.cpp
        tads3/vminitim.cpp tads3/vmintcls.cpp tads3/vmisaac.cpp tads3/vmiter.cpp
        tads3/vmlog.cpp tads3/vmlookup.cpp tads3/vmlst.cpp tads3/vmmain.cpp
        tads3/vmmcreg.cpp tads3/vmmeta.cpp tads3/vmnetfillcl.cpp tads3/vmobj.cpp
        tads3/vmop.cpp tads3/vmpack.cpp tads3/vmpat.cpp tads3/vmpool.cpp
        tads3/vmpoolim.cpp tads3/vmregex.cpp tads3/vmrun.cpp tads3/vmrunsym.cpp
        tads3/vmsa.cpp tads3/vmsave.cpp tads3/vmsort.cpp tads3/vmsortv.cpp
        tads3/vmsrcf.cpp tads3/vmstack.cpp tads3/vmstr.cpp tads3/vmstrbuf.cpp
        tads3/vmstrcmp.cpp tads3/vmtmpfil.cpp tads3/vmtobj.cpp tads3/vmtype.cpp
        tads3/vmtypedh.cpp tads3/vmtz.cpp tads3/vmtzobj.cpp tads3/vmundo.cpp
        tads3/vmvec.cpp)

    list(APPEND srcs tads3/vmconnom.cpp)

    set(macros GARGOYLE VMGLOB_STRUCT GLK_UNICODE GLK_TIMERS TC_TARGET_T3 RUNTIME)
    if(MINGW32)
        list(APPEND macros DMSDOS DT_WIN32)
    endif()

    terp(tadsr
        SRCS ${srcs}
        MACROS ${macros}
        CXXSTD 11
        POSIX)

    if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
        # This is a style choice in the code.
        target_compile_options(tadsr PRIVATE "-Wno-logical-not-parentheses")
    endif()

    target_include_directories(tadsr PRIVATE glk tads2 tads3)
endif()
