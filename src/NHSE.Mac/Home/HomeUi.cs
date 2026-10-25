using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Shared state of the home window: the save, the selected player and the "unsaved changes" signal.
/// </summary>
public sealed class HomeContext(HorizonSave sav, Window owner)
{
    public HorizonSave SAV { get; } = sav;
    public Window Owner { get; } = owner;
    public int PlayerIndex { get; set; }
    public Player Player => SAV.Players[PlayerIndex];

    /// <summary>
    /// Raised whenever something in the save was changed.
    /// </summary>
    public event Action? Changed;

    public void MarkDirty() => Changed?.Invoke();

    /// <summary>
    /// Opens one of the editors as a sheet of the home window; returns true (and marks the save as changed) if it was saved.
    /// </summary>
    public async Task<bool> Edit(Window dialog)
    {
        var ok = await dialog.ShowDialog<bool>(Owner);
        if (ok)
            MarkDirty();
        return ok;
    }
}

/// <summary>
/// Building blocks of the home pages: cards with an icon, a title and a plain-language hint.
/// </summary>
public static class HomeUi
{
    /// <summary>
    /// One of the game's menu icons (NHSE.Sprites/Resources/MenuIcon).
    /// </summary>
    public static Image Icon(string name, double size = 36) => new()
    {
        Source = ItemSprite.GetResource($"sprites/MenuIcon/{name}.png") ?? ItemSprite.Leaf,
        Width = size,
        Height = size,
        VerticalAlignment = VerticalAlignment.Top,
    };

    public static TextBlock Text(string text, params string[] classes)
    {
        var t = new TextBlock { Text = text, TextWrapping = TextWrapping.Wrap };
        t.Classes.AddRange(classes);
        return t;
    }

    public static TextBlock Section(string text) => Text(text, "section");

    public static Border Card(Control content) => new() { Child = content, Classes = { "card" } };

    /// <summary>
    /// Icon + title + hint, with the optional controls below.
    /// </summary>
    public static Grid CardHeader(string icon, string title, string? hint, params Control[] below)
    {
        var text = new StackPanel { Spacing = 2 };
        text.Children.Add(Text(title, "cardtitle"));
        if (!string.IsNullOrEmpty(hint))
            text.Children.Add(Text(hint, "subtle"));
        foreach (var c in below)
            text.Children.Add(c);

        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 12 };
        g.Children.Add(Icon(icon));
        Grid.SetColumn(text, 1);
        g.Children.Add(text);
        return g;
    }

    /// <summary>
    /// A whole card that opens something when clicked.
    /// </summary>
    public static Button ActionCard(string icon, string title, string? hint, Func<Task> onClick, params Control[] below)
    {
        var b = new Button { Content = CardHeader(icon, title, hint, below), Classes = { "card" } };
        b.Click += async (_, _) => await onClick();
        return b;
    }

    public static Button SmallButton(string text, Func<Task> onClick, bool accent = false)
    {
        var b = new Button { Content = text, Padding = new Thickness(12, 4) };
        if (accent)
            b.Classes.Add("accent");
        b.Click += async (_, _) => await onClick();
        return b;
    }

    /// <summary>
    /// Cards in equal columns; each row is as tall as its own tallest card (not as the tallest of all).
    /// </summary>
    public static Grid Tiles(int columns, params Control[] items)
    {
        var g = new Grid { ColumnSpacing = 12, RowSpacing = 12 };
        for (int c = 0; c < columns; c++)
            g.ColumnDefinitions.Add(new ColumnDefinition(GridLength.Star));
        for (int r = 0; r < (items.Length + columns - 1) / columns; r++)
            g.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        for (int i = 0; i < items.Length; i++)
        {
            Grid.SetColumn(items[i], i % columns);
            Grid.SetRow(items[i], i / columns);
            g.Children.Add(items[i]);
        }
        return g;
    }

    /// <summary>
    /// A figure the player can change: icon, small label, the number in big (editable in place) and a caption.
    /// All stat cards have the same parts, so they line up.
    /// </summary>
    public static Border Stat(string icon, string label, NumericUpDown amount, string caption)
    {
        var text = new StackPanel { Spacing = 0, VerticalAlignment = VerticalAlignment.Center };
        text.Children.Add(Text(label, "statlabel"));
        text.Children.Add(amount);
        text.Children.Add(Text(caption, "statcaption"));

        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 14 };
        var image = Icon(icon, 44);
        image.VerticalAlignment = VerticalAlignment.Center;
        g.Children.Add(image);
        Grid.SetColumn(text, 1);
        g.Children.Add(text);
        return Card(g);
    }

    public static WrapPanel Row(params Control[] items)
    {
        var p = new WrapPanel { ItemSpacing = 8, LineSpacing = 6, Margin = new Thickness(0, 6, 0, 0) };
        foreach (var i in items)
            p.Children.Add(i);
        return p;
    }

    /// <summary>
    /// Page layout: title, optional hint, then the content, scrolling as a whole.
    /// </summary>
    public static ScrollViewer Page(string? title, string? hint, params Control[] content)
    {
        var p = new StackPanel { Margin = new Thickness(28, 22, 28, 28) };
        if (title is not null)
            p.Children.Add(Text(title, "title"));
        if (hint is not null)
            p.Children.Add(new TextBlock { Text = hint, Classes = { "subtle" }, Margin = new Thickness(0, 4, 0, 0), TextWrapping = TextWrapping.Wrap });
        if (title is not null && content.Length != 0 && content[0] is not TextBlock)
            content[0].Margin = new Thickness(0, 18, 0, 0);
        foreach (var c in content)
            p.Children.Add(c);
        return new ScrollViewer { Content = p, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled };
    }

    /// <summary>
    /// Number box for an amount: thousands separators, no spinner.
    /// </summary>
    public static NumericUpDown Amount(uint value, uint max = int.MaxValue) => new()
    {
        Value = Math.Min(value, max),
        Minimum = 0,
        Maximum = max,
        FormatString = "N0",
        ShowButtonSpinner = false,
        HorizontalAlignment = HorizontalAlignment.Stretch,
        Classes = { "amount" },
    };

    /// <summary>
    /// Small strip of item sprites (preview of a pocket or a villager's furniture).
    /// </summary>
    public static WrapPanel ItemStrip(IEnumerable<Item> items, int max = 10, double size = 26)
    {
        var p = new WrapPanel { ItemSpacing = 2, LineSpacing = 2, Margin = new Thickness(0, 6, 0, 0) };
        foreach (var item in items.Where(z => !z.IsNone).Take(max))
        {
            var img = new Image { Source = ItemSprite.GetItemSprite(item), Width = size, Height = size };
            ToolTip.SetTip(img, GameInfo.Strings.GetItemName(item));
            p.Children.Add(img);
        }
        return p;
    }
}
