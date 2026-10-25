using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The game opened from the console over Wi-Fi (79-…): the dialog of the launcher and the home window in live mode
/// (no console needed: nothing is sent until «Mandar a la consola»).
/// </summary>
public class GalleryHomeLive
{
    [AvaloniaFact]
    public void OpenLiveDialog()
    {
        var main = new MainWindow();
        main.Show();
        _ = LiveOpen.Open(main);
        Dispatcher.UIThread.RunJobs();
        var dialog = main.OwnedWindows.First(z => z.Title == HomeStrings.OpenLive);
        Gallery.Capture(dialog, "79-open-live");
        main.Close();
    }

    [AvaloniaFact]
    public void HomeInLiveMode()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var w = new HomeWindow(TestSave.LoadCopy(out _), new LiveGame());
        w.Show();
        Dispatcher.UIThread.RunJobs();
        Assert.Contains(HomeStrings.LiveTitleSuffix, w.Title);
        Assert.Equal(HomeStrings.LiveSaveButton, w.FindControl<Button>("B_Save")!.Content);
        Assert.Contains(w.GetVisualDescendants().OfType<Button>(), b => Equals(b.Content, HomeStrings.LiveReload));
        Gallery.Capture(w, "79-home-live", false);

        // The cards that connect to the console again are left out: the whole app already sends to it.
        var nav = w.FindControl<ListBox>("LB_Nav")!;
        for (int i = 0; i < nav.ItemCount; i++)
        {
            nav.SelectedIndex = i;
            Dispatcher.UIThread.RunJobs();
            var texts = w.GetVisualDescendants().OfType<TextBlock>().Select(z => z.Text).ToHashSet();
            Assert.DoesNotContain(HomeStrings.LiveGameCard, texts);
            Assert.DoesNotContain(HomeStrings.LiveCard, texts);
        }
        w.Close();

        // …and they are there when the save was opened from files (so the check above looks in the right place).
        var files = new HomeWindow(TestSave.LoadCopy(out _));
        files.Show();
        var fileNav = files.FindControl<ListBox>("LB_Nav")!;
        var all = new HashSet<string?>();
        for (int i = 0; i < fileNav.ItemCount; i++)
        {
            fileNav.SelectedIndex = i;
            Dispatcher.UIThread.RunJobs();
            all.UnionWith(files.GetVisualDescendants().OfType<TextBlock>().Select(z => z.Text));
        }
        Assert.Contains(HomeStrings.LiveGameCard, all);
        Assert.Contains(HomeStrings.LiveCard, all);
        files.Close();
    }

    /// <summary> Reopened with what came from the console: it says so, and it is not in the files until saved. </summary>
    [AvaloniaFact]
    public void ReopenedWithTheConsoleState()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var w = new HomeWindow(TestSave.LoadCopy(out _));
        w.Show();
        var date = new DateTime(2026, 10, 25, 21, 30, 0);
        w.OpenedWithCopy(date);
        Dispatcher.UIThread.RunJobs();
        Assert.True(w.Dirty);
        var status = w.FindControl<TextBlock>("L_Status")!.Text;
        Assert.Contains(date.ToString("HH:mm"), status);
        Assert.StartsWith(HomeStrings.StatusFromConsole.Split('(')[0], status);
        Gallery.Capture(w, "79-home-reopened");
    }
}
