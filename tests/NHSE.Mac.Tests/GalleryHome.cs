using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The friendly home window, one capture per sidebar entry (70-…).
/// </summary>
public class GalleryHome
{
    [AvaloniaFact]
    public void HomePages()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var w = new HomeWindow(TestSave.LoadCopy(out _));
        w.Show();
        var nav = w.FindControl<ListBox>("LB_Nav")!;
        for (int i = 0; i < nav.ItemCount; i++)
        {
            nav.SelectedIndex = i;
            Dispatcher.UIThread.RunJobs();
            Gallery.Capture(w, $"70-home-{i}", false);
        }
        w.Close();
    }

    [AvaloniaFact]
    public void ItemsWindowPockets()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var sav = TestSave.LoadCopy(out _);
        var pers = sav.Players[0].Personal;
        var items = pers.Pocket.Concat(pers.Bag).ToArray();
        var w = new ItemsWindow(items, 10, 4, HomeStrings.Pockets);
        Gallery.Capture(w, "72-items-pockets");
    }

    [AvaloniaFact]
    public void ItemsWindowStorage()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var sav = TestSave.LoadCopy(out _);
        var w = new ItemsWindow(sav.Players[0].Personal.ItemChest, 10, 5, HomeStrings.Storage);
        Gallery.Capture(w, "73-items-storage");
    }

    [AvaloniaFact]
    public void VillagerPickerWindow()
    {
        var w = new VillagerPicker();
        w.Select("cat01");
        Gallery.Capture(w, "71-villager-picker");
    }
}
