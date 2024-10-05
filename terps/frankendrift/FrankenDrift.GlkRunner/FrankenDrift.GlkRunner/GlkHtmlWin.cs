using FrankenDrift.GlkRunner.Glk;
using System.Text;
using System.Text.RegularExpressions;

using LinkRef = System.Tuple<System.Range, string>;

namespace FrankenDrift.GlkRunner
{
    /// <summary>
    /// Exception indicating concurrent Glk input requests
    /// </summary>
    internal class ConcurrentEventException : Exception
    {
        public ConcurrentEventException(string msg) : base(msg) { }
    }

    // Making the best out of the limited set of Glk styles...
    [Flags]
    enum TextStyle : uint
    {
        Normal    = 0b00000,
        Bold      = 0b00001,
        Italic    = 0b00010,
        Monospace = 0b00100,
        Centered  = 0b01000
    }
    struct FontInfo
    {
        internal TextStyle Ts;
        internal uint TextColor;
        internal string TagName;
    }

    internal partial class GlkHtmlWin : Glue.RichTextBox, IDisposable
    {
        private readonly IGlk GlkApi;
        private readonly GlkUtil GlkUtil;
        internal static GlkHtmlWin? MainWin = null;
        internal static uint NumberOfWindows = 0;

        private WindowHandle glkwin_handle;
        private static bool _imagesSupported;
        private static bool _hyperlinksSupported;
        private static bool _colorSupported;

        public int TextLength => -1;
        public string Text { get => ""; set { } }
        public string SelectedText { get => ""; set { } }
        public int SelectionStart { get => -1; set { } }
        public int SelectionLength { get => -1; set { } }
        public bool IsDisposed => glkwin_handle.IsValid;

        internal bool IsWaiting = false;
        private string _pendingText = "";
        private readonly Dictionary<uint, string> _hyperlinks = new();
        private uint _hyperlinksSoFar = 1;
        internal bool DoSbAutoHyperlinks => Adrift.SharedModule.Adventure.Title == "Skybreak";

        private string _mostRecentText = "";

        static readonly string[] Monospaces = {
            "Andale Mono", "Cascadia Code", "Century Schoolbook Monospace", "Consolas", "Courier", "Courier New",
            "Liberation Mono", "Ubuntu Mono", "DejaVu Sans Mono",
            "Droid Sans Mono", "Lucida Console", "Menlo", "OCR-A", "OCR-A extended", "Overpass Mono", "Oxygen Mono",
            "Roboto Mono", "Source Code Pro", "Everson Mono", "Fira Mono", "Fixed", "Fixedsys", "FreeMono", "Go Mono",
            "HyperFont", "IBM MDA", "IBM Plex Mono", "Inconsolata", "Iosevka", "Letter Gothic", "Monaco", "Monofur",
            "Monospace", "Monospace (Unicode)", "Nimbus Mono L", "Noto Mono", "NK57 Monospace", "OCR-B", "PragmataPro",
            "Prestige Elite", "ProFont", "PT Mono", "Spleen", "Terminus", "Tex Gyre Cursor", "American Typewriter",
            "TADS-monospace"
        };

        internal StreamHandle Stream => GlkApi.glk_window_get_stream(glkwin_handle);

        internal StreamHandle EchoStream
        {
            get => GlkApi.glk_window_get_echo_stream(glkwin_handle);
            set => GlkApi.glk_window_set_echo_stream(glkwin_handle, value);
        }
        internal bool IsEchoing => EchoStream.IsValid;

        internal GlkHtmlWin(IGlk glk)
        {
            GlkApi = glk;
            GlkUtil = new GlkUtil(GlkApi);
            if (MainWin is null)
            {
                MainWin = this;
                var noWin = new WindowHandle(IntPtr.Zero);
                glkwin_handle = GlkApi.glk_window_open(noWin, 0, 0, WinType.TextBuffer, NumberOfWindows);
                _imagesSupported = GlkApi.glk_gestalt(Gestalt.Graphics, 0) != 0 && GlkApi.glk_gestalt(Gestalt.DrawImage, (uint)WinType.TextBuffer) != 0;
                _hyperlinksSupported = GlkApi.glk_gestalt(Gestalt.Hyperlinks, 0) != 0 && GlkApi.glk_gestalt(Gestalt.HyperlinkInput, (uint)WinType.TextBuffer) != 0;
                _colorSupported = GlkApi.glk_gestalt(Gestalt.GarglkText, 0) != 0;
            }
            else
            {
                glkwin_handle = DoWindowOpen(GlkApi, MainWin, WinType.TextBuffer, WinMethod.Right | WinMethod.Proportional, 30);
            }
        }

        GlkHtmlWin(IGlk glk, GlkHtmlWin splitFrom, WinMethod splitMethod, uint splitSize)
        {
            GlkApi = glk;
            GlkUtil = new GlkUtil(GlkApi);
            glkwin_handle = DoWindowOpen(GlkApi, splitFrom, WinType.TextBuffer, splitMethod, splitSize);
        }

        private static WindowHandle DoWindowOpen(IGlk glk, GlkHtmlWin splitFrom, WinType type, WinMethod splitMethod, uint splitSize)
        {
            var result = glk.glk_window_open(splitFrom.glkwin_handle, splitMethod, splitSize, type, ++NumberOfWindows);
            if (!result.IsValid)
                throw new GlkError("Failed to open window.");
            return result;
        }

        internal GlkGridWin CreateStatusBar()
        {
            return new GlkGridWin(GlkApi, DoWindowOpen(GlkApi, this, WinType.TextGrid, WinMethod.Above | WinMethod.Fixed, 1));
        }

        public void Clear()
        {
            if (_colorSupported)
                GlkApi.garglk_set_zcolors((uint)ZColor.Default, (uint)ZColor.Default);
            GlkApi.glk_window_clear(glkwin_handle);
        }

        internal unsafe string GetLineInput()
        {
            if (IsWaiting)
                throw new ConcurrentEventException("Too many input events requested");
            IsWaiting = true;
            const uint capacity = 256;
            var cmdToBe = new uint[capacity];
            fixed (uint* buf = cmdToBe)
            {
                GlkApi.glk_request_line_event_uni(glkwin_handle, buf, capacity-1, 0);
                if (_hyperlinks.Count > 0 && _hyperlinksSupported)
                    GlkApi.glk_request_hyperlink_event(glkwin_handle);
                while (true)
                {
                    Event ev = new() { type = EventType.None };
                    GlkApi.glk_select(ref ev);
                    if (ev.type == EventType.LineInput && ev.win_handle == glkwin_handle)
                    {
                        var count = (int) ev.val1;
                        GlkApi.glk_cancel_hyperlink_event(glkwin_handle);
                        IsWaiting = false;
                        var result = new StringBuilder(count);
                        for (var i = 0; i < count; i++)
                            result.Append(new Rune(buf[i]).ToString());
                        return result.ToString();
                    }
                    else if (ev.type == EventType.Hyperlink && ev.win_handle == glkwin_handle)
                    {
                        var linkId = ev.val1;
                        Event ev2 = new();
                        if (_hyperlinks.TryGetValue(linkId, out string? result))
                        {
                            GlkApi.glk_cancel_line_event(glkwin_handle, ref ev2);
                            IsWaiting = false;
                            _hyperlinks.Clear();
                            FakeInput(result);
                            return result;
                        }
                    }
                    else MainSession.Instance!.ProcessEvent(ev);
                }
            }
        }

        internal uint GetCharInput()
        {
            if (IsWaiting)
                throw new ConcurrentEventException("Too many input events requested");
            IsWaiting = true;
            uint result;
            GlkApi.glk_request_char_event(glkwin_handle);
            while (true)
            {
                Event ev = new() { type = EventType.None };
                GlkApi.glk_select(ref ev);
                if (ev.type == EventType.CharInput && ev.win_handle == glkwin_handle)
                {
                    result = ev.val1;
                    break;
                }
                else MainSession.Instance!.ProcessEvent(ev);
            }
            IsWaiting = false;
            return result;
        }

        private void FakeInput(string cmd)
        {
            GlkApi.glk_set_window(glkwin_handle);
            if (_colorSupported)
                GlkApi.garglk_set_zcolors((uint)ZColor.Default, (uint)ZColor.Default);
            GlkApi.glk_set_style(Style.Input);
            GlkUtil.OutputString(cmd);
            GlkApi.glk_set_style(Style.Normal);
            GlkUtil.OutputString("\n");
        }

        // Janky-ass HTML parser, 2nd edition.
        public void AppendHTML(string src)
        {
            if (string.IsNullOrEmpty(src))
                return;
            // don't echo back the command, the Glk library already takes care of that
            if (src.StartsWith("<c><font face=\"Wingdings\" size=14>") && src.EndsWith("</c>\r\n"))
                return;
            // strip redundant carriage-return characters
            src = src.Replace("\r\n", "\n");
            if (IsWaiting)
            {
                _pendingText += src;
                return;
            }
            GlkApi.glk_set_window(glkwin_handle);
            if (_colorSupported)
                GlkApi.garglk_set_zcolors((uint)ZColor.Default, (uint)ZColor.Default);

            var consumed = 0;
            var inToken = false;
            var current = new StringBuilder();
            var currentToken = "";
            var skip = 0;
            var styleHistory = new Stack<FontInfo>();
            styleHistory.Push(new FontInfo { Ts = TextStyle.Normal, TextColor = (uint)ZColor.Default, TagName = "<base>" });

            var linksToBe = new Queue<LinkRef>();
            if (_hyperlinksSupported && DoSbAutoHyperlinks)
            {
                var linkTargets = LinkTargetRegex().Matches(src);
                if (linkTargets.Count == 0)
                    goto NoSuitableLinkTargetsFound;
                for (var i = 0; i < linkTargets.Count; i++)
                {
                    var choice = linkTargets[i];
                    if (choice is not { Success: true }) continue;
                    linksToBe.Enqueue(new LinkRef(new Range(choice.Index, choice.Index + choice.Length), choice.Groups[1].Value));
                }
            }

            NoSuitableLinkTargetsFound:

            LinkRef? nextLinkRef = null;
            if (linksToBe.Count > 0)
                nextLinkRef = linksToBe.Dequeue();

            foreach (var c in src)
            {
                consumed++;
                if (skip-- > 0) continue;

                if (c == '<' && !inToken)
                {
                    inToken = true;
                    currentToken = "";
                }
                else if (c != '>' && inToken)
                {
                    currentToken += c;
                }
                else if (c == '>' && inToken)
                {
                    var currentTextStyle = styleHistory.Peek();
                    inToken = false;
                    var tokenLower = currentToken.ToLower();
                    if (tokenLower == "del")  // Attempt to remove a character from the output.
                    {
                        if (current.Length > 0) {
                            // As long as we haven't committed to displaying anything,
                            // we can simply remove the last character from the buffer.
                            // (Will break on characters that don't fit into the Unicode BMP, but alas.)
                            current.Remove(current.Length - 1, 1);
                        }
                        else if (!string.IsNullOrEmpty(_mostRecentText))
                        {
                            // Under Garglk, we can still recall a character that has already been output.
                            // This seems like a potentially expensive operation, but shouldn't happen too often.
                            if (GlkUtil.UnputChar((uint)_mostRecentText.EnumerateRunes().Last().Value)) {
                                StringBuilder rebuilder = new(_mostRecentText.Length);
                                foreach (var rune in _mostRecentText.EnumerateRunes().SkipLast(1))
                                    rebuilder.Append(rune.ToString());
                                _mostRecentText = rebuilder.ToString();
                            }
                        }
                        continue;
                    }
                    OutputStyled(current.ToString(), currentTextStyle);
                    current.Clear();
                    switch (tokenLower)
                    {
                        case "br":
                            GlkUtil.OutputString("\n");
                            break;
                        case "c":  // Keep the 'input' style reserved for actual input, only mimic it with color where possible.
                            styleHistory.Push(new FontInfo { Ts = currentTextStyle.Ts, TextColor = GetInputTextColor(), TagName = "c" });
                            break;
                        case "b":
                            styleHistory.Push(new FontInfo { Ts = currentTextStyle.Ts |= TextStyle.Bold, TextColor = currentTextStyle.TextColor, TagName = "b" });
                            break;
                        case "i":
                            styleHistory.Push(new FontInfo { Ts = currentTextStyle.Ts |= TextStyle.Italic, TextColor = currentTextStyle.TextColor, TagName = "i" });
                            break;
                        case "center":
                            styleHistory.Push(new FontInfo { Ts = currentTextStyle.Ts |= TextStyle.Centered, TextColor = currentTextStyle.TextColor, TagName = "center" });
                            break;
                        case "tt":
                            styleHistory.Push(new FontInfo { Ts = currentTextStyle.Ts |= TextStyle.Monospace, TextColor = currentTextStyle.TextColor, TagName = "tt" });
                            break;
                        case "/c" when currentTextStyle.TagName == "c":
                        case "/b" when currentTextStyle.TagName == "b":
                        case "/i" when currentTextStyle.TagName == "i":
                        case "/center" when currentTextStyle.TagName == "center":
                        case "/tt" when currentTextStyle.TagName == "tt":
                        case "/font" when currentTextStyle.TagName == "font":
                            styleHistory.Pop();
                            break;
                        case "cls":
                            styleHistory.Clear();
                            styleHistory.Push(new FontInfo { Ts = TextStyle.Normal, TextColor = (uint)ZColor.Default, TagName = "<base>" });
                            Clear();
                            break;
                        case "waitkey":
                            GetCharInput();
                            break;
                    }
                    if (tokenLower.StartsWith("font"))
                    {
                        var color = currentTextStyle.TextColor;
                        var col = ColorRegex().Match(tokenLower);
                        if (col.Success)
                        {
                            if (uint.TryParse(col.Groups[1].Value, System.Globalization.NumberStyles.HexNumber, null, out var newColor))
                                color = newColor;
                        }
                        else if (tokenLower.Contains("black"))
                            color = 0x000000;
                        else if (tokenLower.Contains("blue"))
                            color = 0x0000ff;
                        else if (tokenLower.Contains("gray"))
                            color = 0x808080;
                        else if (tokenLower.Contains("darkgreen"))
                            color = 0x006400;
                        else if (tokenLower.Contains("green"))
                            color = 0x008000;
                        else if (tokenLower.Contains("lime"))
                            color = 0x00FF00;
                        else if (tokenLower.Contains("magenta"))
                            color = 0xFF00FF;
                        else if (tokenLower.Contains("maroon"))
                            color = 0x800000;
                        else if (tokenLower.Contains("navy"))
                            color = 0x000080;
                        else if (tokenLower.Contains("olive"))
                            color = 0x808000;
                        else if (tokenLower.Contains("orange"))
                            color = 0xffa500;
                        else if (tokenLower.Contains("pink"))
                            color = 0xffc0cb;
                        else if (tokenLower.Contains("purple"))
                            color = 0x800080;
                        else if (tokenLower.Contains("red"))
                            color = 0xff0000;
                        else if (tokenLower.Contains("silver"))
                            color = 0xc0c0c0;
                        else if (tokenLower.Contains("teal"))
                            color = 0x008080;
                        else if (tokenLower.Contains("white"))
                            color = 0xffffff;
                        else if (tokenLower.Contains("yellow"))
                            color = 0xffff00;
                        else if (tokenLower.Contains("cyan"))
                            color = 0x00ffff;
                        else if (tokenLower.Contains("darkolive"))
                            color = 0x556b2f;
                        else if (tokenLower.Contains("tan"))
                            color = 0xd2b48c;

                        var currstyle = currentTextStyle.Ts;
                        var face = FontFaceRegex().Match(tokenLower);
                        if (face.Success)
                        {
                            var f = face.Groups[1].Value;
                            if (Monospaces.Any(msf => msf.Equals(f, StringComparison.InvariantCultureIgnoreCase)))
                                currstyle |= TextStyle.Monospace;
                        }

                        styleHistory.Push(new FontInfo { Ts = currstyle, TextColor = color, TagName = "font" });
                    }
                    else if (tokenLower.StartsWith("img") && _imagesSupported)
                    {
                        var imgPath = SrcRegex().Match(currentToken);
                        if (imgPath.Success && Adrift.SharedModule.Adventure.BlorbMappings is { Count: > 0 }
                                && Adrift.SharedModule.Adventure.BlorbMappings.TryGetValue(imgPath.Groups[1].Value, out int res))
                        {
                            GlkApi.glk_image_draw(glkwin_handle, (uint)res, (int)ImageAlign.MarginRight, 0);
                        }
                    }
                    else if (tokenLower.StartsWith("audio play"))
                    {
                        var sndPath = SrcRegex().Match(currentToken);
                        if (!sndPath.Success) continue;
                        var channel = 1;
                        var chanMatch = ChannelRegex().Match(currentToken);
                        if (chanMatch.Success)
                        {
                            channel = int.Parse(chanMatch.Groups[1].Value);
                            if (channel is > 8 or < 1) continue;
                        }
                        var loop = tokenLower.Contains("loop=y");
                        MainSession.Instance!.PlaySound(sndPath.Groups[1].Value, channel, loop);
                    }
                    else if (tokenLower.StartsWith("audio pause"))
                    {
                        var m = ChannelRegex().Match(currentToken);
                        if (m.Success)
                        {
                            var ch = int.Parse(m.Groups[1].Value);
                            if (ch is > 8 or < 1) continue;
                            MainSession.Instance!.PauseSound(ch);
                        }
                        else MainSession.Instance!.PauseSound(1);
                    }
                    else if (tokenLower.StartsWith("audio stop"))
                    {
                        var m = ChannelRegex().Match(currentToken);
                        if (m.Success)
                        {
                            var ch = int.Parse(m.Groups[1].Value);
                            if (ch is > 8 or < 1) continue;
                            MainSession.Instance!.StopSound(ch);
                        }
                        else MainSession.Instance!.StopSound(1);
                    }
                }
                else if (nextLinkRef is not null && nextLinkRef.Item1.Start.Value == consumed)
                {
                    OutputStyled(current.ToString(), styleHistory.Peek());
                    current.Clear();
                    _hyperlinks[_hyperlinksSoFar] = nextLinkRef.Item2;
                    GlkApi.glk_set_hyperlink(_hyperlinksSoFar++);
                    current.Append(c);
                }
                else if (nextLinkRef is not null && nextLinkRef.Item1.End.Value == consumed)
                {
                    current.Append(c);
                    OutputStyled(current.ToString(), styleHistory.Peek());
                    current.Clear();
                    GlkApi.glk_set_hyperlink(0);
                    if (linksToBe.Count > 0)
                        nextLinkRef = linksToBe.Dequeue();
                }
                else current.Append(c);
            }

            OutputStyled(current.ToString(), styleHistory.Peek());
            if (_hyperlinksSupported)
                GlkApi.glk_set_hyperlink(0);
            if (_imagesSupported)
                GlkApi.glk_window_flow_break(glkwin_handle);
            if (_colorSupported)
                GlkApi.garglk_set_zcolors((uint)ZColor.Default, (uint)ZColor.Default);

            if (!IsWaiting && !string.IsNullOrEmpty(_pendingText))
            {
                var tmpText = _pendingText;
                _pendingText = "";
                AppendHTML(tmpText);
            }
        }

        internal bool DrawImageImmediately(uint imgId)
        {
            if (!_imagesSupported) return false;
            var result = GlkApi.glk_image_draw(glkwin_handle, imgId, (int)ImageAlign.MarginLeft, 0);
            if (result == 0) return false;
            GlkApi.glk_window_flow_break(glkwin_handle);
            return true;
        }

        private void OutputStyled(string txt, FontInfo fi)
        {
            if (string.IsNullOrEmpty(txt)) return;
            txt = txt.Replace("&lt;", "<").Replace("&gt;", ">").Replace("&perc;", "%").Replace("&quot;", "\"");
            if (_colorSupported)
                GlkApi.garglk_set_zcolors(fi.TextColor, (uint)ZColor.Default);
            if ((fi.Ts & TextStyle.Monospace) != 0)
            {
                GlkApi.glk_set_style(Style.Preformatted);
            }
            else if ((fi.Ts & TextStyle.Centered) != 0)
            {
                GlkApi.glk_set_style(Style.BlockQuote);
            }
            else if ((fi.Ts & TextStyle.Italic) != 0 && (fi.Ts & TextStyle.Bold) != 0)
            {
                GlkApi.glk_set_style(Style.Alert);
            }
            else if ((fi.Ts & TextStyle.Italic) != 0)
            {
                GlkApi.glk_set_style(Style.Emphasized);
            }
            else if ((fi.Ts & TextStyle.Bold) != 0)
            {
                GlkApi.glk_set_style(Style.Subheader);
            }
            else
            {
                GlkApi.glk_set_style(Style.Normal);
            }
            _mostRecentText = txt;
            GlkUtil.OutputString(txt);
        }

        private uint GetInputTextColor()
        {
            uint result = 0;
            var success = GlkApi.glk_style_measure(glkwin_handle, Style.Input, StyleHint.TextColor, ref result);
            if (success == 0)
                return (uint)ZColor.Default;
            return result;
        }

        protected virtual void Dispose(bool disposing)
        {
            if (glkwin_handle.IsValid)
            {
                if (disposing)
                {
                    // TODO: Verwalteten Zustand (verwaltete Objekte) bereinigen
                }

                // TODO: Nicht verwaltete Ressourcen (nicht verwaltete Objekte) freigeben und Finalizer überschreiben
                // TODO: Große Felder auf NULL setzen

                // we're not interested in the results, but alas...
                StreamResult r = new();
                if (IsEchoing)
                    GlkApi.glk_stream_close(EchoStream, ref r);
                GlkApi.glk_window_close(glkwin_handle, ref r);
                glkwin_handle = new(IntPtr.Zero);
            }
        }

        ~GlkHtmlWin()
        {
            // Ändern Sie diesen Code nicht. Fügen Sie Bereinigungscode in der Methode "Dispose(bool disposing)" ein.
            Dispose(disposing: false);
        }

        public void Dispose()
        {
            // Ändern Sie diesen Code nicht. Fügen Sie Bereinigungscode in der Methode "Dispose(bool disposing)" ein.
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }

        internal void DumpCurrentStyleInfo()
        {
            foreach (Style s in (Style[])Enum.GetValues(typeof(Style)))
            {
                uint result = 0;
                AppendHTML($"Style status for style: {s}\n");
                AppendHTML("  Indentation: ");
                var success = GlkApi.glk_style_measure(glkwin_handle, s, StyleHint.Indentation, ref result);
                AppendHTML(success == 1 ? $"{result}\n" : "n/a\n");
                AppendHTML("  Paragraph Indentation: ");
                success = GlkApi.glk_style_measure(glkwin_handle, s, StyleHint.ParaIndentation, ref result);
                AppendHTML(success == 1 ? $"{result}\n" : "n/a\n");
                AppendHTML("  Justification: ");
                success = GlkApi.glk_style_measure(glkwin_handle, s, StyleHint.Justification, ref result);
                AppendHTML(success == 1 ? $"{(Justification)result}\n" : "n/a\n");
                AppendHTML("  Font Size: ");
                success = GlkApi.glk_style_measure(glkwin_handle, s, StyleHint.Size, ref result);
                AppendHTML(success == 1 ? $"{result}\n" : "n/a\n");
                AppendHTML("  Font Weight: ");
                success = GlkApi.glk_style_measure(glkwin_handle, s, StyleHint.Weight, ref result);
                AppendHTML(success == 1 ? $"{(int)result}\n" : "n/a\n");
                AppendHTML("  Italics: ");
                success = GlkApi.glk_style_measure(glkwin_handle, s, StyleHint.Oblique, ref result);
                if (success == 1) AppendHTML(result == 1 ? "yes\n" : "no\n");
                else AppendHTML("n/a\n");
                AppendHTML("  Font Type: ");
                success = GlkApi.glk_style_measure(glkwin_handle, s, StyleHint.Proportional, ref result);
                if (success == 1) AppendHTML(result == 1 ? "proportional\n" : "fixed-width\n");
                else AppendHTML("n/a\n");
                AppendHTML("  Text color: ");
                success = GlkApi.glk_style_measure(glkwin_handle, s, StyleHint.TextColor, ref result);
                AppendHTML(success == 1 ? $"0x{result:X}\n" : "n/a\n");
            }
        }

        [GeneratedRegex("src ?= ?\"(.+)\"", RegexOptions.IgnoreCase)]
        private static partial Regex SrcRegex();
        [GeneratedRegex("channel=(\\d)", RegexOptions.IgnoreCase)]
        private static partial Regex ChannelRegex();
        [GeneratedRegex("face ?= ?\"(.*?)\"", RegexOptions.IgnoreCase)]
        private static partial Regex FontFaceRegex();
        [GeneratedRegex("^([0-9a-zA-Z]+?)\\) .+$", RegexOptions.Multiline | RegexOptions.IgnoreCase)]
        private static partial Regex LinkTargetRegex();
        [GeneratedRegex("color ?= ?\"?#?([0-9A-Fa-f]{6})\"?", RegexOptions.IgnoreCase)]
        private static partial Regex ColorRegex();
    }
}
