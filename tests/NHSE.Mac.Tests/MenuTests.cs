using Avalonia;
using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The macOS menu bar: app menu, launcher menu and editor menu.
/// </summary>
public class MenuTests
{
    private static string[] Headers(NativeMenu? menu) => menu?.Items.OfType<NativeMenuItem>().Select(z => z.Header ?? "").ToArray() ?? [];

    [AvaloniaFact]
    public void AppMenuHasAboutAndUpdates()
    {
        var menu = NativeMenu.GetMenu(Application.Current!);
        Assert.Equal(2, Headers(menu).Count());
    }

    [AvaloniaFact]
    public void LauncherHasFileAndOptions()
    {
        var w = new MainWindow();
        var top = NativeMenu.GetMenu(w)!.Items.OfType<NativeMenuItem>().ToArray();
        Assert.Equal(2, top.Length);
        Assert.Contains(MacStrings.MenuOpenFolder, Headers(top[0].Menu));
        Assert.Contains(MacStrings.MenuLanguage, Headers(top[1].Menu));
    }

    [AvaloniaFact]
    public void EditorHasFileToolsAndOptions()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var w = new Editor(TestSave.LoadCopy(out _));
        var top = NativeMenu.GetMenu(w)!.Items.OfType<NativeMenuItem>().ToArray();
        Assert.Equal(3, top.Length);
        Assert.Contains(Translator.Tr("Editor.Menu_Save", "Save"), Headers(top[0].Menu));
        Assert.Contains(Translator.Tr("Editor.Menu_VerifyHashes", "Verify Hashes"), Headers(top[1].Menu));
        Assert.Contains(MacStrings.MenuLanguage, Headers(top[2].Menu));
    }
}
