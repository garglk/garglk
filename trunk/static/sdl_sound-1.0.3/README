SDL_sound. An abstract soundfile decoder.

SDL_sound is a library that handles the decoding of several popular sound file
 formats, such as .WAV and .MP3. It is meant to make the programmer's sound
 playback tasks simpler. The programmer gives SDL_sound a filename, or feeds
 it data directly from one of many sources, and then reads the decoded
 waveform data back at her leisure. If resource constraints are a concern,
 SDL_sound can process sound data in programmer-specified blocks. Alternately,
 SDL_sound can decode a whole sound file and hand back a single pointer to the
 whole waveform. SDL_sound can also handle sample rate, audio format, and
 channel conversion on-the-fly and behind-the-scenes, if the programmer
 desires.

Please check the website for the most up-to-date information about SDL_sound:
   http://icculus.org/SDL_sound/

SDL_sound _REQUIRES_ Simple Directmedia Layer (SDL) to function, and cannot
 be built without it. You can get SDL from http://www.libsdl.org/. SDL_sound
 has only been tried with the SDL 1.2 series, but may work on older versions.
 Reports of success or failure are welcome.

Some optional external libraries that SDL_sound can use and where to find them:
 SMPEG (used to decode MP3s): http://icculus.org/smpeg/
 libvorbisfile (used to decode OGGs): http://www.xiph.org/ogg/vorbis/
 libSpeex (used to decode SPXs): http://speex.org/
 libFLAC (used to decode FLACs): http://flac.sourceforge.net/
 libModPlug (used to decode MODs, etc): http://modplug-xmms.sourceforge.net/
 libMikMod (used to decode MODs, etc, too): http://www.mikmod.org/

 Experimental QuickTime support for the Mac is included, but has not been 
 integrated with the build system, and probably doesn't work with 
 QuickTime for Windows.

These external libraries are OPTIONAL. SDL_sound will build and function
 without them, but various sound file formats are not supported unless these
 libraries are available. Unless explicitly disabled during initial build
 configuration, SDL_sound always supports these file formats internally:

 - Microsoft .WAV files (uncompressed and MS-ADPCM encoded).
 - Creative Labs .VOC files
 - Shorten (.SHN) files
 - Audio Interchange format (AIFF) files
 - Sun Audio (.AU) files
 - MIDI files
 - MP3 files (internal decoder, different than the one SMPEG uses)
 - Raw waveform data

Building/Installing:
  Please read the INSTALL document.

Reporting bugs/commenting:
 There is a mailing list available. To subscribe, send a blank email to
 sdlsound-subscribe@icculus.org. This is the best way to get in touch with
 SDL_sound developers.

--ryan. (icculus@icculus.org)


