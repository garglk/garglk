using System;
using System.Text.RegularExpressions;
using FrankenDrift.Glue.Infragistics.Win.UltraWinToolbars;

namespace FrankenDrift.Glue
{
    public static partial class Util
    {
        public static string StripCarats(string str) => StripCarets(str);
        public static string StripCarets(string str)
        {
            if (str.Contains("<#") && str.Contains("#>"))
                str = str.Replace("<#", "[[==~~").Replace("#>", "~~==]]");
            return HtmlTagRegex().Replace(str, "");
        }

        public static string GetSetting(string _a, string _b, string _c, string _d = "") => _d;
        public static void SaveSetting(string _a, string _b, string _c, string _val) { }
        public static void DeleteSetting(string _a, string _b, string _c) {  }

        public static void AddPrevious() { }
        public static void AddPrevious(UltraToolbarsManager _a, string _b) { }

        [GeneratedRegex("<(.|\n)+?>")]
        private static partial Regex HtmlTagRegex();
    }

    public static class Application
    {
        static UIGlue _frontend;
        public static void SetFrontend(UIGlue app)
        {
            _frontend = app;
        }

        public static string LocalUserAppDataPath =>
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        public static string StartupPath => _frontend.GetExecutableLocation();
        public static string ExecutablePath => _frontend.GetExecutablePath();
        public static string ProductVersion => _frontend.GetClaimedAdriftVersion();

        public static void DoEvents() => _frontend.DoEvents();
    }
}
