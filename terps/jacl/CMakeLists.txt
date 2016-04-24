set(SRCS jacl.c glk_startup.c findroute.c interpreter.c loader.c glk_saver.c logging.c parser.c display.c utils.c jpp.c resolvers.c errors.c encapsulate.c libcsv.c)

interpreter(jacl "${SRCS}")
target_link_libraries(jacl m)
target_compile_definitions(jacl PRIVATE GLK)
