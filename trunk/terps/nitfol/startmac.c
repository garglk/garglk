#line 576 "opt2glkc.pl"
#include "nitfol.h"
#include "macglk_startup.h"

static strid_t mac_gamefile;

static BOOL hashandle = FALSE;
static AliasHandle gamehandle;
#line 694 "opt2glkc.pl"
strid_t startup_findfile(void)
{
  ;
}
#line 586 "opt2glkc.pl"
strid_t intd_filehandle_open(strid_t savefile, glui32 operating_id,
                             glui32 contents_id, glui32 interp_id,
                             glui32 length)
{
  FSSpec file;
  Boolean wasChanged;
  if(operating_id != 0x4d414353 /* 'MACS' */)
    return 0;
  if(contents_id != 0)
    return 0;
  if(interp_id != 0x20202020 /* '    ' */)
    return 0;

  gamehandle = NewHandle(length);
  glk_get_buffer_stream(savefile, *gamehandle, length);
  hashandle = TRUE;
  ResolveAlias(NULL, gamehandle, &file, &wasChanged);
  return macglk_stream_open_fsspec(&file, 0, 0);  
}

void intd_filehandle_make(strid_t savefile)
{
  if(!hashandle)
    return;
  glk_put_string_stream(savefile, "MACS");
  glk_put_char_stream(savefile, b00000010); /* Flags */
  glk_put_char_stream(savefile, 0);         /* Contents ID */
  glk_put_char_stream(savefile, 0);         /* Reserved */
  glk_put_char_stream(savefile, 0);         /* Reserved */
  glk_put_string_stream(savefile, "    ");/* Interpreter ID */
  glk_put_buffer_stream(savefile, *gamehandle, *gamehandle->aliasSize);
}

glui32 intd_get_size(void)
{
  if(!hashandle)
    return 0;
  return *gamehandle->aliasSize + 12;
}

static Boolean mac_whenselected(FSSpec *file, OSType filetype)
{
  NewAlias(NULL, file, &gamehandle);
  hashandle = TRUE;
  return game_use_file(mac_gamefile);
}

static Boolean mac_whenbuiltin()
{
  return game_use_file(mac_gamefile);
}

Boolean macglk_startup_code(macglk_startup_t *data)
{
  OSType mac_gamefile_types[] = { 0x5a434f44 /* 'ZCOD' */, 0x49465253 /* 'IFRS' */, 0x49465a53 /* 'IFZS' */ };

  data->startup_model  = macglk_model_ChooseOrBuiltIn;
  data->app_creator    = 0x6e695466 /* 'niTf' */;
  data->gamefile_types = mac_gamefile_types;
  data->num_gamefile_types = sizeof(mac_gamefile_types) / sizeof(*mac_gamefile_types);
  data->savefile_type  = 0x49465a53 /* 'IFZS' */;
  data->datafile_type  = 0x5a697044 /* 'ZipD' */;
  data->gamefile       = &mac_gamefile;
  data->when_selected  = mac_whenselected;
  data->when_builtin   = mac_whenbuiltin;

  return TRUE;
}
