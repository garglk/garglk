set(srcs buffer.c err.c fastmem.c files.c input.c main.c math.c object.c process.c quetzal.c random.c redirect.c sound.c stream.c table.c text.c variable.c glkscreen.c glkmisc.c)

interpreter(frotz "${srcs}")
