using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The event flags explained (87-…, 88-…): every known flag has an explanation, changes are kept on saving and
/// put back on cancelling.
/// </summary>
public class GalleryFlags
{
    private static HorizonSave Save()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        return TestSave.LoadCopy(out _);
    }

    [AvaloniaFact]
    public void PlayerFlags()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var flags = pers.GetEventFlagsPlayer();
        var w = new EventFlagsWindow(flags, false, string.Format(FlagStrings.PlayerTitle, pers.PlayerName));
        Gallery.Capture(w, "87-flags-player", false);

        var row = w.Shown.First(r => r.IsYesNo);
        var before = row.IsOn;
        row.IsOn = !before;
        w.SaveAndClose();
        pers.SetEventFlagsPlayer(flags);
        Assert.Equal(before ? 0 : 1, pers.GetEventFlagsPlayer()[row.Index]);
    }

    [AvaloniaFact]
    public void IslandFlagsSearchAndCancel()
    {
        var sav = Save();
        var flags = sav.Main.EventFlagLand.ToArray();
        var copy = flags.ToArray();
        var w = new EventFlagsWindow(flags, true, FlagStrings.IslandTitle);
        w.Show();
        Dispatcher.UIThread.RunJobs();
        w.Search(GameInfo.CurrentLanguage == "es" ? "museo" : "museum");
        Assert.NotEmpty(w.Shown);
        Gallery.Capture(w, "88-flags-island-search", false);

        foreach (var r in w.Shown.Where(z => z.IsYesNo))
            r.IsOn = !r.IsOn;
        w.Close();
        Assert.Equal(copy, flags);
    }

    [AvaloniaFact]
    public void EveryFlagIsExplained()
    {
        if (GameInfo.CurrentLanguage != "es")
            return;
        foreach (var index in EventFlagPlayer.List.Keys)
            Assert.False(string.IsNullOrWhiteSpace(FlagDescriptions.Get(false, index)), $"player {index}");
        foreach (var index in EventFlagLand.List.Keys)
            Assert.False(string.IsNullOrWhiteSpace(FlagDescriptions.Get(true, index)), $"island {index}");
    }
}
