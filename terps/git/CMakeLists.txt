set(SRCS git.c memory.c compiler.c opcodes.c operands.c peephole.c terp.c glkop.c search.c git_unix.c savefile.c saveundo.c gestalt.c heap.c accel.c)

interpreter(git "${SRCS}")
target_link_libraries(git m)
target_compile_definitions(git PRIVATE USE_INLINE USE_OWN_POWF)
