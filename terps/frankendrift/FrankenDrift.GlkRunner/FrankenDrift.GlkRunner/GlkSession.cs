using FrankenDrift.GlkRunner.Glk;
using FrankenDrift.Glue;
using FrankenDrift.Glue.Infragistics.Win.UltraWinToolbars;
using System.Runtime.InteropServices;
using System.Text;

namespace FrankenDrift.GlkRunner
{
    public class MainSession : Glue.UIGlue, frmRunner
    {
        internal static MainSession? Instance = null;
        private readonly IGlk GlkApi;
        private GlkHtmlWin? _output;
        private GlkGridWin? _status;
        private readonly bool _soundSupported;

        public UltraToolbarsManager UTMMain => throw new NotImplementedException();
        public RichTextBox txtOutput => _output;
        public RichTextBox txtInput => _output;  // huh?
        public bool Locked => false;
        public void Close() => GlkApi.glk_exit();

        private readonly Dictionary<int, SoundChannel> _sndChannels = new();
        private readonly Dictionary<int, string> _recentlyPlayedSounds = new();

        public MainSession(string gameFile, IGlk glk)
        {
            if (Instance is not null)
                throw new ApplicationException("Dual MainSessions?");
            Instance = this;
            GlkApi = glk;

            var util = new GlkUtil(GlkApi);
            if (!util._unicodeAvailable)
            {
                _output = new(glk);
                _output.AppendHTML("Sorry, can't run with a non-unicode Glk library.\n<waitkey>\n");
                Environment.Exit(2);
            }

            // If playing a blorb file, open it with the Glk library as well
            if (gameFile.EndsWith(".blorb"))
            {
                var blorb = File.ReadAllBytes(gameFile);
                // Blorb files produced by ADRIFT have the wrong file length in the header
                // for some reason, so passing it to Glk will not work unless we do this terribleness:
                var length = blorb.Length - 8;
                var lengthBytes = BitConverter.GetBytes(length);
                if (BitConverter.IsLittleEndian)
                    Array.Reverse(lengthBytes);
                for (int i = 0; i < 4; i++)
                    blorb[i + 4] = lengthBytes[i];
                var tmpFileRef = GlkApi.glk_fileref_create_temp(FileUsage.Data | FileUsage.BinaryMode, 0);
                var tmpFileStream = GlkApi.glk_stream_open_file(tmpFileRef, Glk.FileMode.ReadWrite, 0);
                GlkApi.glk_put_buffer_stream(tmpFileStream, blorb, (uint)blorb.Length);
                GlkApi.glk_fileref_destroy(tmpFileRef);
                GlkApi.glk_stream_set_position(tmpFileStream, 0, SeekMode.Start);
                GlkApi.giblorb_set_resource_map(tmpFileStream);
            }

            Adrift.SharedModule.Glue = this;
            Adrift.SharedModule.fRunner = this;
            Glue.Application.SetFrontend(this);
            Adrift.SharedModule.UserSession = new Adrift.RunnerSession { Map = new GlonkMap(), bShowShortLocations = true };
            for (int i = 1; i <= 8; i++)
                _sndChannels[i] = GlkApi.glk_schannel_create((uint)i);
            Adrift.SharedModule.UserSession.OpenAdventure(gameFile);
            _soundSupported = GlkApi.glk_gestalt(Gestalt.Sound2, 0) != 0;
            // The underlying Runner wants a tick once per second to trigger real-time-based events
            if (GlkApi.glk_gestalt(Gestalt.Timer, 0) != 0)
                GlkApi.glk_request_timer_events(1000);
        }

        public void Run()
        {
            while (true)
            {
                var cmd = _output.GetLineInput();
                SubmitCommand(cmd.Trim());
            }
        }

        internal void ProcessEvent(Event ev)
        {
            switch (ev.type)
            {
                case EventType.LineInput:
                    SubmitCommand();
                    break;
                case EventType.Timer:
                    // For what little good it does us -- the output window will be tied up waiting for input,
                    // so any text that gets output won't be seen until the user has sent off the command they
                    // are currently editing. But this will still cause anything other than text output to happen.
                    Adrift.SharedModule.UserSession.TimeBasedStuff();
                    break;
                default:
                    break;
            }
        }

        public bool AskYesNoQuestion(string question, string title = null)
        {
            if (_output is null)
                _output = new(GlkApi);
            if (title is not null)
                _output.AppendHTML($"\n<font color=\"red\"><b>{title}</b></font>");
            _output.AppendHTML($"\n<font color=\"red\">{question}</font>");
            while (true)
            {
                _output.AppendHTML("\n[yes/no] > ");
                var result = _output.GetLineInput().ToLower();
                switch (result)
                {
                    case "y":
                    case "yes":
                        return true;
                    case "n":
                    case "no":
                        return false;
                }
            }
        }

        public void DoEvents()
        {
            GlkApi.glk_tick();
        }

        public void EnableButtons() { }

        public void ErrMsg(string message, Exception ex = null)
        {
            if (_output is null)
                _output = new(GlkApi);
            _output.AppendHTML($"<b>ADRIFT Fatal Error: {message}</b><br>");
            if (ex is not null)
                _output.AppendHTML(ex.ToString());
        }

        public string GetAppDataPath()
        {
            throw new NotImplementedException();
        }

        public string GetClaimedAdriftVersion()
        {
            return "5.0000364";
        }

        public string GetExecutableLocation()
        {
            return Path.GetDirectoryName(GetExecutablePath());
        }

        public string GetExecutablePath()
        {
            return Environment.ProcessPath;
        }

        public void InitInput()
        { }

        public bool IsTranscriptActive() => false;

        public void MakeNote(string msg)
        {
            if (_output is not null)
                _output.AppendHTML($"ADRIFT Note: {msg}");
        }

        public void OutputHTML(string source) => _output.AppendHTML(source);

        public string QueryRestorePath()
        {
            var fileref = GlkApi.glk_fileref_create_by_prompt(FileUsage.SavedGame | FileUsage.BinaryMode, Glk.FileMode.Read, 0);
            if (!fileref.IsValid) return "";
            var result = GlkApi.glkunix_fileref_get_name(fileref);
            GlkApi.glk_fileref_destroy(fileref);
            return result;
        }

        public QueryResult QuerySaveBeforeQuit()
        {
            throw new NotImplementedException();
        }

        public string QuerySavePath()
        {
            var fileref = GlkApi.glk_fileref_create_by_prompt(FileUsage.SavedGame | FileUsage.BinaryMode, Glk.FileMode.Write, 0);
            if (!fileref.IsValid) return "";
            var result = GlkApi.glkunix_fileref_get_name(fileref);
            GlkApi.glk_fileref_destroy(fileref);
            return result;
        }

        public void ReloadMacros() { }

        public void SaveLayout() { }

        public void ScrollToEnd() { }

        public void SetBackgroundColour()
        {
            var adventure = Adrift.SharedModule.Adventure;
            if (adventure.DeveloperDefaultBackgroundColour != 0)
            {
                int colorToBe = adventure.DeveloperDefaultBackgroundColour & 0x00FFFFFF;
                foreach (Style s in (Style[]) Enum.GetValues(typeof(Style)))
                    GlkApi.glk_stylehint_set(WinType.AllTypes, s, StyleHint.BackColor, colorToBe);
            }
            if (adventure.DeveloperDefaultOutputColour != 0 && adventure.DeveloperDefaultOutputColour != adventure.DeveloperDefaultBackgroundColour)
            {
                int colorToBe = adventure.DeveloperDefaultOutputColour & 0x00FFFFFF;
                foreach (Style s in (Style[]) Enum.GetValues(typeof(Style)))
                {
                    if (s == Style.Input) continue;
                    GlkApi.glk_stylehint_set(WinType.AllTypes, s, StyleHint.TextColor, colorToBe);
                }
            }
            if (adventure.DeveloperDefaultInputColour != 0 && adventure.DeveloperDefaultInputColour != adventure.DeveloperDefaultBackgroundColour)
            {
                int colorToBe = adventure.DeveloperDefaultInputColour & 0x00FFFFFF;
                GlkApi.glk_stylehint_set(WinType.AllTypes, Style.Input, StyleHint.TextColor, colorToBe);
            }

            // Repurpose blockquote style for centered text (seems the most appropriate out of the bunch)
            GlkApi.glk_stylehint_set(WinType.AllTypes, Style.BlockQuote, StyleHint.Justification, (int)Justification.Centered);

            // It is perhaps bad form / unexpected to do this here, but we can't
            // open any windows until after the style hints have been adjusted.
            _output = new GlkHtmlWin(GlkApi);
            _status = _output.CreateStatusBar();
        }

        public void SetGameName(string name)
        {
            GlkApi.SetGameName(name);
        }

        public void ShowCoverArt(byte[] img)
        {
            // we don't need the image data that the interpreter has provided us,
            // we just need to ask Glk to display image no. 1
            _output.DrawImageImmediately(1);
        }

        public void ShowInfo(string info, string title = null)
        {
            throw new NotImplementedException();
        }

        public void SubmitCommand()
        {
            // not sure what should go here
        }

        internal void SubmitCommand(string cmd)
        {
            cmd = cmd.Trim(' ');
            if (cmd == "!dumpstyles")
            {
                _output!.DumpCurrentStyleInfo();
                return;
            }
            Adrift.SharedModule.UserSession.Process(cmd);
            Adrift.SharedModule.Adventure.Turns += 1;
        }

        public void UpdateStatusBar(string desc, string score, string user)
        {
            if (_status is null) return;
            if (string.IsNullOrEmpty(user))
            {
                desc = Adrift.SharedModule.ReplaceALRs(desc);
                score = Adrift.SharedModule.ReplaceALRs(score);
                var winWidth = _status.Width;
                var spaces = winWidth - desc.Length - score.Length - 1;
                if (spaces < 2) spaces = 2;
                _status.RewriteStatus(desc + new string(' ', spaces) + score);
            }
            else
            {
                desc = Adrift.SharedModule.ReplaceALRs(desc);
                score = Adrift.SharedModule.ReplaceALRs(score);
                user = Adrift.SharedModule.ReplaceALRs(user);
                var winWidth = _status.Width;
                var spaces = (winWidth - desc.Length - score.Length - 1) / 2;
                if (spaces < 2) spaces = 2;
                _status.RewriteStatus(desc + new string(' ', spaces) + score + new string(' ', spaces) + user);
            }
        }

        internal void PlaySound(string snd, int channel, bool loop)
        {
            if (!_soundSupported) return;
            if (_recentlyPlayedSounds.TryGetValue(channel, out string? value) && value == snd)
            {
                UnpauseSound(channel);
                return;
            }
            if (!(Adrift.SharedModule.Adventure.BlorbMappings is { Count: > 0 })
                    || !Adrift.SharedModule.Adventure.BlorbMappings.TryGetValue(snd, out int theSound))
                return;
            _recentlyPlayedSounds[channel] = snd;
            GlkApi.glk_schannel_play_ext(_sndChannels[channel], (uint)theSound, loop ? 0xFFFFFFFF : 1, 0);
        }

        private void UnpauseSound(int channel)
        {
            if (!_soundSupported) return;
            GlkApi.glk_schannel_unpause(_sndChannels[channel]);
        }

        internal void PauseSound(int channel)
        {
            if (!_soundSupported) return;
            GlkApi.glk_schannel_pause(_sndChannels[channel]);
        }

        internal void StopSound(int channel)
        {
            if (!_soundSupported) return;
            GlkApi.glk_schannel_stop(_sndChannels[channel]);
            if (_recentlyPlayedSounds.ContainsKey(channel))
                _recentlyPlayedSounds.Remove(channel);
        }
    }

    // Glonk: does absolutely nothing and dies.
    // see: https://en.wikipedia.org/wiki/Flanimals#:~:text=Glonk%3A%20A%20green%20reptilian%20humanoid,known%20that%20it%20eats%20pizza.
    class GlonkMap : Glue.Map
    {
        public void RecalculateNode(object node) { }

        public void SelectNode(string key) { }
    }
}