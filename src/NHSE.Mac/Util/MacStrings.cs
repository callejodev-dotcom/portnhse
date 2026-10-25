// ReSharper disable AutoPropertyCanBeMadeGetOnly.Global
namespace NHSE.Mac;

/// <summary>
/// Strings that only exist in the macOS port (the rest come from NHSE's own translation files).
/// Translated with <c>Resources/MacStrings_xx.txt</c>; languages without a file use these English defaults.
/// </summary>
public static class MacStrings
{
    public static string MsgTitleError { get; set; } = "Error";
    public static string MsgTitleAlert { get; set; } = "NHSE";
    public static string MsgTitlePrompt { get; set; } = "NHSE";

    public static string LauncherDrop { get; set; } = "Drop your save folder here";
    public static string LauncherHint { get; set; } = "The folder that contains main.dat, mainHeader.dat and the Villager0… folders, or a .zip of it.";
    public static string LauncherOpenFolder { get; set; } = "Open Save Folder…";
    public static string LauncherOpenFile { get; set; } = "Open main.dat or .zip…";
    public static string LauncherReopen { get; set; } = "Reopen Last";
    public static string LauncherBackups { get; set; } = "Show Backups";
    public static string LauncherWarning { get; set; } = "Bring your own save data: this program does not dump it from your console. Keep a copy before editing.";

    public static string MenuFile { get; set; } = "File";
    public static string MenuOpenFolder { get; set; } = "Open Save Folder…";
    public static string MenuOpenFile { get; set; } = "Open main.dat or .zip…";
    public static string MenuCloseWindow { get; set; } = "Close Window";
    public static string MenuLanguage { get; set; } = "Language";
    public static string MenuAbout { get; set; } = "About NHSE";
    public static string MenuQuit { get; set; } = "Quit NHSE";

    public static string MsgBackupMac { get; set; } = "NHSE can keep a copy of every save you open, in its backups folder ({0}).";
    public static string MsgAbout { get; set; } = "macOS port of NHSE, the Animal Crossing: New Horizons save editor by Kaphotics and contributors. Licensed under the GPLv3.";

    public static string HintItemGrid { get; set; } = "Click: view · ⇧ Click: set · ⌥ Click: delete · ⌘⌥ Click: clone to all · Right click: menu";
    public static string MsgNotPorted { get; set; } = "This editor has not been ported to macOS yet.";
}
