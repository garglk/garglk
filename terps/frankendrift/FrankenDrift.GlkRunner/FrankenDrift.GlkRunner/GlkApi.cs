using System.Data.Common;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace FrankenDrift.GlkRunner
{
    public class GlkError : Exception
    {
        public GlkError(string what) : base(what) { }
    }
}

namespace FrankenDrift.GlkRunner.Glk
{
    public enum BlorbError : uint
    {
        None = 0,
        CompileTime = 1,
        Alloc = 2,
        Read = 3,
        NotAMap = 4,
        Format = 5,
        NotFound = 6
    }

    enum CharOutput : uint
    {
        CannotPrint = 0,
        ApproxPrint = 1,
        ExactPrint = 2
    }

    public enum Gestalt : uint
    {
        Version = 0,
        CharInput = 1,
        LineInput = 2,
        CharOutput = 3,
        MouseInput = 4,
        Timer = 5,
        Graphics = 6,
        DrawImage = 7,
        Sound = 9,
        SoundVolume = 10,
        Hyperlinks = 11,
        HyperlinkInput = 12,
        SoundMusic = 13,
        GraphicsTransparency = 14,
        Unicode = 15,
        UnicodeNorm = 16,
        LineInputEcho = 17,
        LineTerminators = 18,
        LineTerminatorKey = 19,
        DateTime = 20,
        Sound2 = 21,
        ResourceStream = 22,
        GraphicsCharInput = 23,
        GarglkText = 0x1100
    }

    enum EventType : uint
    {
        None = 0,
        Timer = 1,
        CharInput = 2,
        LineInput = 3,
        MouseInput = 4,
        Arrange = 5,
        Redraw = 6,
        SoundNotify = 7,
        Hyperlink = 8,
        VolumeNotify = 9
    }

    public enum FileMode : uint
    {
        Write = 0x01,
        Read = 0x02,
        ReadWrite = 0x03,
        WriteAppend = 0x05
    }

    [Flags]
    public enum FileUsage : uint
    {
#pragma warning disable CA1069 // Enums values should not be duplicated
        Data = 0x00,
        SavedGame = 0x01,
        Transcript = 0x02,
        InputRecord = 0x03,
        TypeMask = 0x0f,
        TextMode = 0x100,
        BinaryMode = 0x000
#pragma warning restore CA1069 // Enums values should not be duplicated
    }

    enum ImageAlign : int
    {
        InlineUp = 1,
        InlineDown = 2,
        InlineCenter = 3,
        MarginLeft = 4,
        MarginRight = 5
    }

    enum Justification : uint
    {
        LeftFlush = 0,
        LeftRight = 1,
        Centered = 2,
        RightFlush = 3
    }

    public enum SeekMode : uint
    {
        Start = 0,
        Current = 1,
        End = 2
    }

    public enum Style : uint
    {
        Normal = 0,
        Emphasized = 1,
        Preformatted = 2,
        Header = 3,
        Subheader = 4,
        Alert = 5,
        Note = 6,
        BlockQuote = 7,
        Input = 8,
        User1 = 9,
        User2 = 10
    }

    public enum StyleHint : uint
    {
        Indentation = 0,
        ParaIndentation = 1,
        Justification = 2,
        Size = 3,
        Weight = 4,
        Oblique = 5,
        Proportional = 6,
        TextColor = 7,
        BackColor = 8,
        ReverseColor = 9
    }

    [Flags]
    public enum WinMethod : uint
    {
#pragma warning disable CA1069 // Enums values should not be duplicated
        Left = 0x00,
        Right = 0x01,
        Above = 0x02,
        Below = 0x03,
        DirMask = 0x0f,
        Fixed = 0x10,
        Proportional = 0x20,
        DivisionMask = 0xf0,
        Border = 0x000,
        NoBorder = 0x100,
        BorderMask = 0x100
#pragma warning restore CA1069 // Enums values should not be duplicated
    }

    public enum WinType : uint
    {
        AllTypes = 0,
        Pair = 1,
        Blank = 2,
        TextBuffer = 3,
        TextGrid = 4,
        Graphics = 5
    }

    enum ZColor : uint
    {
        Transparent = 0xfffffffc,
        Cursor = 0xfffffffd,
        Current = 0xfffffffe,
        Default = 0xffffffff
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Event
    {
        internal EventType type;
        internal WindowHandle win_handle;
        internal uint val1;
        internal uint val2;
    }

    [StructLayout(LayoutKind.Sequential)]
    public readonly struct StreamResult
    {
        public readonly uint readcount;
        public readonly uint writecount;
    }

    [StructLayout(LayoutKind.Sequential)]
    public record struct WindowHandle(IntPtr hwnd)
    {
        internal bool IsValid => hwnd != IntPtr.Zero;
    }

    [StructLayout(LayoutKind.Sequential)]
    public record struct FileRefHandle(IntPtr hfref)
    {
        internal bool IsValid => hfref != IntPtr.Zero;
    }

    [StructLayout(LayoutKind.Sequential)]
    public record struct StreamHandle(IntPtr hstrm)
    {
        internal bool IsValid => hstrm != IntPtr.Zero;
    }

    [StructLayout(LayoutKind.Sequential)]
    public record struct SoundChannel(IntPtr schan)
    {
        internal bool IsValid => schan != IntPtr.Zero;
    }


    public interface IGlk
    {
#pragma warning disable IDE1006 // Naming Styles
        BlorbError giblorb_set_resource_map(StreamHandle fileStream);
        void glk_cancel_hyperlink_event(WindowHandle winId);
        void glk_cancel_line_event(WindowHandle winId, ref Event ev);
        void glk_exit();
        FileRefHandle glk_fileref_create_by_name(FileUsage usage, string name, FileMode fmode, uint rock);
        FileRefHandle glk_fileref_create_by_prompt(FileUsage usage, FileMode fmode, uint rock);
        FileRefHandle glk_fileref_create_temp(FileUsage usage, uint rock);
        void glk_fileref_destroy(FileRefHandle fref);
        uint glk_image_draw(WindowHandle winid, uint imageId, int val1, int val2);
        uint glk_image_get_info(uint imageId, ref uint width, ref uint height);
        void glk_put_buffer(byte[] s, uint len);
        void glk_put_buffer_stream(StreamHandle streamId, byte[] s, uint len);
        void glk_put_buffer_uni(uint[] s, uint len);
        void glk_request_char_event(WindowHandle winId);
        void glk_request_hyperlink_event(WindowHandle winId);
        unsafe void glk_request_line_event(WindowHandle win, byte* buf, uint maxlen, uint initlen);
        unsafe void glk_request_line_event_uni(WindowHandle win, uint* buf, uint maxlen, uint initlen);
        SoundChannel glk_schannel_create(uint rock);
        void glk_schannel_destroy(SoundChannel chan);
        void glk_schannel_pause(SoundChannel chan);
        uint glk_schannel_play(SoundChannel chan, uint sndId);
        uint glk_schannel_play_ext(SoundChannel chan, uint sndId, uint repeats, uint notify);
        void glk_schannel_set_volume(SoundChannel chan, uint vol);
        void glk_schannel_stop(SoundChannel chan);
        void glk_schannel_unpause(SoundChannel chan);
        void glk_select(ref Event ev);
        void glk_set_hyperlink(uint linkval);
        void glk_set_style(Style s);
        void glk_set_window(WindowHandle winId);
        StreamHandle glk_stream_open_file(FileRefHandle fileref, FileMode fmode, uint rock);
        unsafe StreamHandle glk_stream_open_memory(byte* buf, uint buflen, FileMode mode, uint rock);
        void glk_stream_close(StreamHandle stream, ref StreamResult result);
        void glk_stream_set_position(StreamHandle stream, int pos, SeekMode seekMode);
        void glk_stylehint_set(WinType wintype, Style styl, StyleHint hint, int val);
        uint glk_style_measure(WindowHandle winid, Style styl, StyleHint hint, ref uint result);
        void glk_tick();
        void glk_window_clear(WindowHandle winId);
        void glk_window_close(WindowHandle winId, ref StreamResult streamResult);
        void glk_window_flow_break(WindowHandle winId);
        void glk_window_get_size(WindowHandle winId, out uint width, out uint height);
        StreamHandle glk_window_get_stream(WindowHandle winId);
        void glk_window_move_cursor(WindowHandle winId, uint xpos, uint ypos);
        void glk_window_set_echo_stream(WindowHandle winId, StreamHandle stream);
        StreamHandle glk_window_get_echo_stream(WindowHandle winId);
        WindowHandle glk_window_open(WindowHandle split, WinMethod method, uint size, WinType wintype, uint rock);
        void garglk_set_zcolors(uint fg, uint bg);
        string? glkunix_fileref_get_name(FileRefHandle fileref);
        uint glk_gestalt(Gestalt sel, uint val);
        unsafe uint glk_gestalt_ext(Gestalt sel, uint val, uint* arr, uint arrlen);
        void glk_request_timer_events(uint millisecs);

        // And some extra functions we want that could have different implementations
        void SetGameName(string game);
#pragma warning restore IDE1006 // Naming Styles
    }

    public class GlkUtil
    {
        private readonly IGlk GlkApi;
        internal readonly bool _unicodeAvailable;

        public GlkUtil(IGlk glk)
        {
            GlkApi = glk;
            _unicodeAvailable = (GlkApi.glk_gestalt(Gestalt.Unicode, 0) != 0);
        }

        internal void OutputString(string msg)
        {
            if (_unicodeAvailable)
            {
                var runes = msg.EnumerateRunes().Select(r => (uint)r.Value).ToArray();
                GlkApi.glk_put_buffer_uni(runes, (uint)runes.Length);
            }
            else OutputStringLatin1(msg);
        }

        internal void OutputStringLatin1(string msg)
        {
            var encoder = Encoding.GetEncoding(Encoding.Latin1.CodePage, EncoderFallback.ReplacementFallback, DecoderFallback.ReplacementFallback);
            var bytes = encoder.GetBytes(msg);
            GlkApi.glk_put_buffer(bytes, (uint)bytes.Length);
        }
    }
}