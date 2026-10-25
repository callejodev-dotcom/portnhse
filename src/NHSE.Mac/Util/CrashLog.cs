using System.Diagnostics;
using Avalonia.Threading;

namespace NHSE.Mac;

/// <summary>
/// Unexpected errors: written to ~/Library/Logs/NHSE and shown in a message, instead of closing the app.
/// </summary>
public static class CrashLog
{
    public static string Folder { get; } = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Library", "Logs", "NHSE");

    private static bool Showing;

    public static void Install()
    {
        // Exceptions in event handlers (async void included) end up here before they can stop the run loop.
        Dispatcher.UIThread.UnhandledException += (_, e) =>
        {
            e.Handled = true;
            var path = Write(e.Exception);
            Show(e.Exception, path);
        };
        AppDomain.CurrentDomain.UnhandledException += (_, e) => Write(e.ExceptionObject as Exception);
        TaskScheduler.UnobservedTaskException += (_, e) =>
        {
            Write(e.Exception);
            e.SetObserved();
        };
    }

    /// <summary>
    /// Writes the exception with the app version and the time; returns the file path.
    /// </summary>
    public static string? Write(Exception? ex)
    {
        if (ex is null)
            return null;
        try
        {
            Directory.CreateDirectory(Folder);
            var path = Path.Combine(Folder, $"NHSE {DateTime.Now:yyyy-MM-dd HH.mm.ss}.txt");
            File.WriteAllText(path, $"NHSE {App.GetVersion()} · macOS {Environment.OSVersion.Version} · {DateTime.Now:O}{Environment.NewLine}{Environment.NewLine}{ex}");
            return path;
        }
        catch (Exception logError)
        {
            Debug.WriteLine(logError);
            return null;
        }
    }

    private static async void Show(Exception ex, string? path)
    {
        if (Showing)
            return;
        Showing = true;
        try
        {
            var where = path is null ? string.Empty : string.Format(MacStrings.MsgErrorLog, UiUtil.ShortPath(path));
            await Dialogs.Error(MacStrings.MsgUnexpectedError, ex.Message, where);
        }
        catch (Exception showError)
        {
            Debug.WriteLine(showError);
        }
        finally
        {
            Showing = false;
        }
    }
}
