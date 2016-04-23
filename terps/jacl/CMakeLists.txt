set(srcs jacl.c glk_startup.c findroute.c interpreter.c loader.c glk_saver.c logging.c parser.c display.c utils.c jpp.c resolvers.c errors.c encapsulate.c libcsv.c)

interpreter(jacl "${srcs}")
target_link_libraries(jacl m)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGLK")
