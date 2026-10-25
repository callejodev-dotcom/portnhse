using Avalonia.Controls;
using Avalonia.Headless;
using Avalonia.Headless.XUnit;
using Avalonia.Input;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

public class ItemEditorTests
{
    private static (Window, ItemEditor) Show()
    {
        var editor = new ItemEditor();
        editor.Initialize(GameInfo.Strings.ItemDataSource);
        var w = new Window { Content = editor, Width = 260, Height = 600 };
        w.Show();
        Dispatcher.UIThread.RunJobs();
        return (w, editor);
    }

    [AvaloniaFact]
    public void LoadAndSetRoundTrip()
    {
        var (w, editor) = Show();
        var item = new Item(0x09C4) { Count = 7 }; // some item id with a count
        editor.LoadItem(item);
        var copy = editor.SetItem(new Item());
        Assert.Equal(item.ItemId, copy.ItemId);
        Assert.Equal((ushort)7, copy.Count);
        w.Close();
    }

    [AvaloniaFact]
    public void TypingAnItemNameSelectsIt()
    {
        var (w, editor) = Show();
        var cb = editor.FindControl<ComboBox>("CB_ItemID")!;
        var name = GameInfo.Strings.ItemDataSource.First(z => z.Value == 0x09C4).Text;
        cb.Focus();
        w.KeyTextInput(name);
        Dispatcher.UIThread.RunJobs();
        Assert.Equal(0x09C4, UiUtil.GetIndex(cb));
        w.Close();
    }
}
