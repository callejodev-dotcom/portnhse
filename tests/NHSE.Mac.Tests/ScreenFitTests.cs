using Avalonia;
using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// A window taller than the screen (e.g. after opening "Advanced options") stops at the screen's usable area,
/// and its Save button is still inside the window.
/// </summary>
public class ScreenFitTests
{
    [AvaloniaFact]
    public void TallWindowFitsTheScreen()
    {
        var owner = new Window { Width = 300, Height = 300 };
        owner.Show();
        var screen = owner.Screens.ScreenFromWindow(owner) ?? owner.Screens.Primary;
        Assert.SkipWhen(screen is null, "no screen in this test host");
        var usable = screen!.WorkingArea.ToRect(screen.Scaling).Height;

        var w = new NhseWindow { Width = 400, SizeToContent = SizeToContent.Height };
        var tall = new StackPanel();
        for (int i = 0; i < 200; i++)
            tall.Children.Add(new TextBlock { Text = $"Line {i}", Height = 20 });
        IslandUi.Frame(w, "Title", "Hint", tall, () => { });
        w.Show(owner);
        Dispatcher.UIThread.RunJobs();

        Assert.True(w.ClientSize.Height < usable, $"{w.ClientSize.Height} >= {usable}");
        var save = w.GetVisualDescendants().OfType<Button>().Single(z => z.IsDefault);
        var bottom = save.TranslatePoint(new Point(0, save.Bounds.Height), w);
        Assert.NotNull(bottom);
        Assert.True(bottom!.Value.Y <= w.ClientSize.Height, $"Save ends at {bottom.Value.Y}, window is {w.ClientSize.Height}");
        w.Close();
        owner.Close();
    }
}
