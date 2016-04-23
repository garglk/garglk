set(srcs git.c memory.c compiler.c opcodes.c operands.c peephole.c terp.c glkop.c search.c git_unix.c savefile.c saveundo.c gestalt.c heap.c accel.c)

interpreter(git "${srcs}")
target_link_libraries(git m)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_INLINE -DUSE_OWN_POWF")
