set(SRCS vmcrc.cpp vmmain.cpp std.cpp charmap.cpp resload.cpp resldexe.cpp vminit.cpp vmini_nd.cpp vmconsol.cpp vmconhmp.cpp vminitim.cpp vmcfgmem.cpp vmobj.cpp vmundo.cpp vmtobj.cpp vmpat.cpp vmstrcmp.cpp vmdict.cpp vmgram.cpp vmstr.cpp vmcoll.cpp vmiter.cpp vmlst.cpp vmsort.cpp vmsortv.cpp vmbignum.cpp vmvec.cpp vmintcls.cpp vmanonfn.cpp vmlookup.cpp vmbytarr.cpp vmcset.cpp vmfilobj.cpp vmstack.cpp vmerr.cpp vmerrmsg.cpp vmpool.cpp vmpoolim.cpp vmtype.cpp vmtypedh.cpp utf8.cpp vmglob.cpp vmrun.cpp vmfunc.cpp vmmeta.cpp vmsa.cpp vmbiftio.cpp vmbif.cpp vmbifl.cpp vmimage.cpp vmimg_nd.cpp vmrunsym.cpp vmsrcf.cpp vmfile.cpp vmbiftad.cpp vmsave.cpp vmbift3.cpp vmbt3_nd.cpp vmregex.cpp vmhosttx.cpp vmhostsi.cpp vmhash.cpp vmmcreg.cpp vmbifreg.cpp vmtmpfil.cpp vmnetfillcl.cpp vmdynfunc.cpp vmstrbuf.cpp vmpack.cpp vmfref.cpp vmlog.cpp vmisaac.cpp md5.cpp sha2.cpp tcmain.cpp tcgen.cpp tcsrc.cpp tctok.cpp tcglob.cpp tcprs.cpp tcprsstm.cpp tcprs_rt.cpp tcprsnf.cpp tcprsnl.cpp tct3.cpp tct3unas.cpp tct3stm.cpp tct3nl.cpp tct3_d.cpp tcerr.cpp tcerrmsg.cpp)

list(APPEND SRCS vmconnom.cpp)

add_library(tads3 STATIC ${SRCS})
tadsdefs(tads3)
