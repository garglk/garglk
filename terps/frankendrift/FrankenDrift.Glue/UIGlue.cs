using System;

namespace FrankenDrift.Glue
{
    public enum QueryResult
    {
        YES,
        NO,
        CANCEL
    }

    public interface UIGlue
    {
        public void ErrMsg(string message, Exception ex = null);
        public void MakeNote(string msg);

        /* Implements:
            fRunner.UTMMain.Tools("OpenGame").SharedProps.Enabled = True
            fRunner.UTMMain.Tools("RestartGame").SharedProps.Enabled = True
            fRunner.UTMMain.Tools("SaveGame").SharedProps.Enabled = True
            fRunner.UTMMain.Tools("SaveGameAs").SharedProps.Enabled = True
            fRunner.UTMMain.Tools("Macros").SharedProps.Enabled = True
            If Not bEXE Then fRunner.UTMMain.Tools("Debugger").SharedProps.Visible = Adventure.EnableDebugger
         */
        public void EnableButtons();

        public void SetGameName(string name);

        // Scrolls to the end of the output window
        public void ScrollToEnd();

        public bool AskYesNoQuestion(string question, string title = null);
        public void ShowInfo(string info, string title = null);


        public string QuerySavePath();
        public string QueryRestorePath();
        public QueryResult QuerySaveBeforeQuit();

        public void OutputHTML(string source);

        public void InitInput();

        public void ShowCoverArt(byte[] img);

        public void DoEvents();
        public string GetAppDataPath();
        public string GetExecutableLocation();
        public string GetExecutablePath();
        public string GetClaimedAdriftVersion();
    }

    public interface frmRunner
    {
        public Infragistics.Win.UltraWinToolbars.UltraToolbarsManager UTMMain { get; }
        public RichTextBox txtOutput { get; }
        public RichTextBox txtInput { get; }

        public void ReloadMacros();
        public void SaveLayout();
        public void SetBackgroundColour();
        public void UpdateStatusBar(string desc, string score, string user);

        public bool Locked { get; }

        public void SetInput(string str) => txtInput.Text = str;
        public void SubmitCommand();

        public void Close();
    }

    public interface RichTextBox
    {
        public void Clear();
        public int TextLength { get; }
        public string Text { get; set; }
        public string SelectedText { get; set; }
        public int SelectionStart { get; set; }
        public int SelectionLength { get; set; }

        public bool IsDisposed { get; }
    }
}
