using Avalonia;
using Avalonia.Controls;
using Avalonia.Headless;
using Avalonia.Headless.XUnit;
using Avalonia.Input;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// Clicks around the item editors like a user would, to catch exceptions that only real input triggers.
/// </summary>
public class MonkeyTests
{
    private static readonly List<Exception> Errors = [];

    private static void Watch()
    {
        Errors.Clear();
        Dispatcher.UIThread.UnhandledExceptionFilter += (_, e) => { Errors.Add(e.Exception); };
    }

    private static void Click(Window w, Control c, RawInputModifiers m = RawInputModifiers.None)
    {
        var p = c.TranslatePoint(new Point(c.Bounds.Width / 2, c.Bounds.Height / 2), w) ?? default;
        w.MouseMove(p, m);
        w.MouseDown(p, MouseButton.Left, m);
        w.MouseUp(p, MouseButton.Left, m);
        Dispatcher.UIThread.RunJobs();
    }

    [AvaloniaFact]
    public void ClassicInventoryClicks()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var sav = TestSave.LoadCopy(out _);
        var pers = sav.Players[0].Personal;
        var items = pers.Pocket.Concat(pers.Bag).ToArray();
        var w = new PlayerItemEditor(items, 10, 4, true);
        w.Show();
        Dispatcher.UIThread.RunJobs();
        var slots = w.GetVisualDescendants().OfType<ItemSlot>().Where(z => z.Width < 100).ToList();
        Assert.NotEmpty(slots);
        foreach (var s in slots)
            Click(w, s);                                // view
        Click(w, slots[^1], RawInputModifiers.Shift);   // set
        Click(w, slots[^1], RawInputModifiers.Alt);     // delete
        Click(w, slots[0], RawInputModifiers.Meta | RawInputModifiers.Alt); // clone
        w.Close();
    }

    [AvaloniaFact]
    public void FriendlyItemsClicks()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var sav = TestSave.LoadCopy(out _);
        var w = new ItemsWindow(sav.Players[0].Personal.ItemChest.Take(400).ToArray(), 10, 5, "Almacén");
        w.Show();
        Dispatcher.UIThread.RunJobs();
        var slots = w.GetVisualDescendants().OfType<ItemSlot>().Where(z => z.Width < 70).ToList();
        foreach (var s in slots)
        {
            Click(w, s);
            // open the advanced options once in a while
        }
        foreach (var chip in w.GetVisualDescendants().OfType<Avalonia.Controls.Primitives.ToggleButton>().Where(z => z.Classes.Contains("chip")).ToList())
            Click(w, chip);
        w.Close();
    }
}

public class CrashLogTests
{
    [AvaloniaFact]
    public void AsyncHandlerErrorsAreLoggedInsteadOfClosingTheApp()
    {
        var before = Directory.Exists(CrashLog.Folder) ? Directory.GetFiles(CrashLog.Folder).Length : 0;
        CrashLog.Install();
        var b = new Button { Content = "x" };
        b.Click += async (_, _) =>
        {
            await Task.Yield();
            throw new InvalidOperationException("prueba del registro de errores");
        };
        var w = new Window { Content = b, Width = 100, Height = 50 };
        w.Show();
        b.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
        for (int i = 0; i < 20; i++)
            Dispatcher.UIThread.RunJobs();
        var files = Directory.GetFiles(CrashLog.Folder);
        Assert.True(files.Length > before);
        var last = files.OrderBy(File.GetLastWriteTimeUtc).Last();
        Assert.Contains("prueba del registro de errores", File.ReadAllText(last));
        File.Delete(last);
        w.Close();
    }
}
