set(srcs automap.c solve.c infix.c copying.c debug.c inform.c quetzal.c undo.c op_call.c decode.c errmesg.c globals.c iff.c init.c io.c z_io.c op_jmp.c op_math.c op_save.c op_table.c op_v6.c oplist.c stack.c zscii.c tokenise.c struct.c objects.c portfunc.c hash.c sound.c graphics.c blorb.c main.c startunix.c)

interpreter(nitfol "${srcs}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSMART_TOKENISER -DDEBUGGING -DFAST -DUSE_INLINE -DNO_TICK")
