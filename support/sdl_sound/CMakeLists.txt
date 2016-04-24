include_directories(. decoders ../sdl)

add_library(sdl_sound STATIC decoders/ogg.c decoders/smpeg.c audio_convert.c alt_audio_convert.c extra_rwops.c SDL_sound.c)

target_compile_definitions(sdl_sound PRIVATE SOUND_SUPPORTS_OGG SOUND_SUPPORTS_SMPEG SDL_SOUND_DLL_EXPORTS)
