using Avalonia.Controls;
using Avalonia.Headless;
using Avalonia.Headless.XUnit;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// Renders the windows to PNG files (galeria/), to check the layouts without opening the app.
/// </summary>
public class Gallery
{
    internal static void Capture(Window w, string name, bool close = true)
    {
        if (!w.IsVisible)
            w.Show();
        Dispatcher.UIThread.RunJobs();
        var frame = w.CaptureRenderedFrame();
        Assert.NotNull(frame);
        frame!.Save(Path.Combine(TestSave.GalleryFolder, name + ".png"), new PngBitmapEncoderOptions());
        if (close)
            w.Close();
    }

    private static HorizonSave Save()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        return TestSave.LoadCopy(out _);
    }

    [AvaloniaFact]
    public void Launcher() => Capture(new MainWindow(), "00-launcher");

    [AvaloniaFact]
    public void EditorTabs()
    {
        var w = new Editor(Save());
        var tabs = w.FindControl<TabControl>("TC_Editors")!;
        for (int i = 0; i < tabs.ItemCount; i++)
        {
            tabs.SelectedIndex = i;
            Capture(w, $"01-editor-{i}", false);
        }
        w.Close();
    }

    [AvaloniaFact]
    public void Inventory()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var items = pers.Pocket.Concat(pers.Bag).ToArray();
        Capture(new PlayerItemEditor(items, 10, 4, true), "02-inventory");
    }

    [AvaloniaFact]
    public void Storage()
    {
        var sav = Save();
        Capture(new PlayerItemEditor(sav.Players[0].Personal.ItemChest, 10, 5), "03-storage");
    }

    [AvaloniaFact]
    public void Turnips()
    {
        var sav = Save();
        Capture(new SingleObjectEditor<TurnipStonk>(sav.Main.Turnips, PropertySort.Categorized, false), "04-turnips");
    }

    [AvaloniaFact]
    public void Visitors()
    {
        var sav = Save();
        Capture(new SingleObjectEditor<object>(sav.Main.Visitor, PropertySort.NoSort, false), "05-visitors");
    }

    [AvaloniaFact]
    public void Settings() => Capture(new SettingsEditor(), "06-settings");
}
