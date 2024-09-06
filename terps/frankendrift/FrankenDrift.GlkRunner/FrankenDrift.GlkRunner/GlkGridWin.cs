using FrankenDrift.GlkRunner.Glk;

namespace FrankenDrift.GlkRunner
{
    internal class GlkGridWin
    {
        private IGlk GlkApi;
        private GlkUtil GlkUtil;
        private WindowHandle glkwin_handle;

        internal GlkGridWin(IGlk glk, WindowHandle handle)
        {
            GlkApi = glk;
            GlkUtil = new GlkUtil(GlkApi);
            glkwin_handle = handle;
        }

        internal void Clear()
        {
            GlkApi.garglk_set_zcolors((uint)ZColor.Default, (uint)ZColor.Default);
            GlkApi.glk_window_clear(glkwin_handle);
        }

        internal void RewriteStatus(string status)
        {
            GlkApi.glk_set_window(glkwin_handle);
            GlkApi.glk_window_move_cursor(glkwin_handle, 0, 0);
            GlkUtil.OutputString(status);
        }

        internal int Width
        {
            get
            {
                GlkApi.glk_window_get_size(glkwin_handle, out var width, out _);
                return (int)width;
            }
        }
    }
}
