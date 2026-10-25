using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// Quitting the app closes every window in some order: the launcher must not try to show itself again.
/// </summary>
public class CloseTests
{
    [AvaloniaFact]
    public async Task QuittingWithAnOpenSaveDoesNotReshowTheClosedLauncher()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        TestSave.LoadCopy(out var folder);
        Program.Settings.BackupPrompted = true;
        Program.Settings.AutomaticBackup = false;

        var main = new MainWindow();
        main.Show();
        Assert.True(await main.TryOpenSaveFile(folder));
        var editor = Assert.Single(main.Editors);
        Dispatcher.UIThread.RunJobs();
        Assert.False(editor.Dirty); // opening a save must not count as a change

        // What ⌘Q does: every window closes, the hidden launcher first.
        main.Close();
        Dispatcher.UIThread.RunJobs();
        editor.Close();
        Dispatcher.UIThread.RunJobs();
        Assert.Empty(main.Editors);
    }
}
