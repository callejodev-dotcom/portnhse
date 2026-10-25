using NHSE.Core;

namespace NHSE.Mac.Tests;

/// <summary>
/// The tests that need real save data read it from the folder in NHSE_TEST_SAVE (never committed: saves are personal data).
/// Each test works on a fresh copy, so the original is never written.
/// </summary>
public static class TestSave
{
    public static string? SourceFolder
    {
        get
        {
            var path = Environment.GetEnvironmentVariable("NHSE_TEST_SAVE");
            return Directory.Exists(path) ? path : null;
        }
    }

    public static bool Available => SourceFolder is not null;

    /// <summary>
    /// Temporary copies of the test save (each one is ~27 MB), one folder per test process so parallel runs
    /// don't delete each other's copies. Emptied when the tests start and when they end.
    /// </summary>
    private static readonly string SavesRoot = Path.Combine(Path.GetTempPath(), "nhse-mac-tests", "saves");
    public static string SavesFolder { get; } = Path.Combine(SavesRoot, Environment.ProcessId.ToString());

    public static void DeleteCopies()
    {
        TryDelete(SavesFolder);
        if (!Directory.Exists(SavesRoot))
            return;
        // Folders of test runs that are no longer running.
        foreach (var dir in Directory.GetDirectories(SavesRoot))
        {
            if (!int.TryParse(Path.GetFileName(dir), out var pid) || !IsRunning(pid))
                TryDelete(dir);
        }
    }

    private static bool IsRunning(int pid)
    {
        try
        {
            using var p = System.Diagnostics.Process.GetProcessById(pid);
            return !p.HasExited;
        }
        catch (ArgumentException)
        {
            return false;
        }
    }

    private static void TryDelete(string dir)
    {
        try
        {
            if (Directory.Exists(dir))
                Directory.Delete(dir, true);
        }
        catch (IOException)
        {
            // In use; a later run cleans up.
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    /// <summary>
    /// Copies the test save to a temporary folder and loads it.
    /// </summary>
    public static HorizonSave LoadCopy(out string folder)
    {
        folder = Path.Combine(SavesFolder, Guid.NewGuid().ToString("N"));
        CopyFolder(SourceFolder!, folder);
        return HorizonSave.FromFolder(folder);
    }

    private static void CopyFolder(string source, string dest)
    {
        Directory.CreateDirectory(dest);
        foreach (var f in Directory.GetFiles(source))
            File.Copy(f, Path.Combine(dest, Path.GetFileName(f)));
        foreach (var d in Directory.GetDirectories(source))
            CopyFolder(d, Path.Combine(dest, Path.GetFileName(d)));
    }

    /// <summary>
    /// Folder where the gallery tests write their screenshots (repo/galeria by default).
    /// </summary>
    public static string GalleryFolder
    {
        get
        {
            var path = Environment.GetEnvironmentVariable("NHSE_GALERIA") ?? Path.Combine(FindRepoRoot(), "galeria");
            Directory.CreateDirectory(path);
            return path;
        }
    }

    private static string FindRepoRoot()
    {
        var dir = AppContext.BaseDirectory;
        while (dir is not null && !File.Exists(Path.Combine(dir, "PortNHSE.slnx")))
            dir = Path.GetDirectoryName(dir);
        return dir ?? Path.GetTempPath();
    }
}
