using Avalonia;
using Avalonia.Controls;
using Avalonia.Headless;
using Avalonia.Headless.XUnit;
using Avalonia.Input;
using Avalonia.Threading;
using Avalonia.VisualTree;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The pockets window with the mouse: an empty space can be chosen, an item put there by clicking it in the list, and
/// dragged onto another empty space.
/// </summary>
public class ItemsWindowMouseTests
{
    private static Point Center(Control c, Window w) => c.TranslatePoint(new Point(c.Bounds.Width / 2, c.Bounds.Height / 2), w)!.Value;

    private static void Click(Window w, Point p)
    {
        w.MouseMove(p, RawInputModifiers.None);
        w.MouseDown(p, MouseButton.Left, RawInputModifiers.None);
        w.MouseUp(p, MouseButton.Left, RawInputModifiers.None);
        Dispatcher.UIThread.RunJobs();
    }

    [AvaloniaFact]
    public void ClickPutAndDragOnEmptySpaces()
    {
        var items = Enumerable.Range(0, 40).Select(_ => new Item(Item.NONE)).ToArray();
        var w = new ItemsWindow(items, 10, 4, "Bolsillos");
        w.Show();
        Dispatcher.UIThread.RunJobs();
        var slots = w.GetVisualDescendants().OfType<ItemSlot>().Take(40).ToList();

        Click(w, Center(slots[13], w));
        Assert.Equal(13, w.SelectedIndex);

        // A click on the first item of the list puts it in the chosen space.
        var row = w.GetVisualDescendants().OfType<ListBoxItem>().First();
        Click(w, Center(row, w));
        Assert.False(items[13].IsNone);
        var id = items[13].ItemId;

        // Dragged onto another empty space.
        var from = Center(slots[13], w);
        var to = Center(slots[25], w);
        w.MouseMove(from, RawInputModifiers.None);
        w.MouseDown(from, MouseButton.Left, RawInputModifiers.None);
        w.MouseMove(from + new Vector(10, 10), RawInputModifiers.LeftMouseButton);
        w.MouseMove(to, RawInputModifiers.LeftMouseButton);
        w.MouseUp(to, MouseButton.Left, RawInputModifiers.None);
        Dispatcher.UIThread.RunJobs();
        Assert.True(items[13].IsNone);
        Assert.Equal(id, items[25].ItemId);
        w.Close();
    }
}
