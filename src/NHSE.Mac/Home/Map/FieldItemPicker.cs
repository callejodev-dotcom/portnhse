using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Picker of the things that grow or stand on the island's ground: trees, bushes, flowers, weeds, fences and rocks.
/// They are named after the item they come from ("pear tree", "red tulips"), and found ignoring case and accents.
/// </summary>
public sealed class FieldItemPicker : UserControl
{
    public sealed record Entry(ushort Id, string Name, string Key, int Group);

    private static string? CachedLanguage;
    private static Entry[] CachedEntries = [];

    private readonly TextBox Search = new() { PlaceholderText = MapStrings.SearchPlant };
    private readonly ListBox Results;

    /// <summary>
    /// Raised with the chosen item.
    /// </summary>
    public event Action<ushort>? ItemChosen;

    public FieldItemPicker(double height = 240)
    {
        Results = new ListBox
        {
            Height = height,
            Margin = new Thickness(0, 6, 0, 0),
            ItemTemplate = new FuncDataTemplate<Entry>((e, _) =>
            {
                var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
                if (e is null)
                    return row;
                row.Children.Add(new Image { Source = GridItems.GetPicture(new Item(e.Id)), Width = 28, Height = 28 });
                row.Children.Add(new TextBlock { Text = e.Name, VerticalAlignment = VerticalAlignment.Center, TextTrimming = TextTrimming.CharacterEllipsis });
                return row;
            }),
        };
        Search.TextChanged += (_, _) => Filter();
        Search.KeyDown += (_, e) =>
        {
            if (e.Key == Key.Enter && Results.ItemCount > 0)
            {
                Choose(Results.Items[0] as Entry);
                e.Handled = true;
            }
            else if (e.Key == Key.Down && Results.ItemCount > 0)
            {
                Results.SelectedIndex = 0;
                Results.Focus();
                e.Handled = true;
            }
        };
        Results.Tapped += (_, _) => Choose(Results.SelectedItem as Entry);
        Results.KeyDown += (_, e) =>
        {
            if (e.Key != Key.Enter)
                return;
            Choose(Results.SelectedItem as Entry);
            e.Handled = true;
        };

        var panel = new StackPanel();
        panel.Children.Add(Search);
        panel.Children.Add(Results);
        Content = panel;
        Filter();
    }

    private void Choose(Entry? e)
    {
        if (e is not null)
            ItemChosen?.Invoke(e.Id);
    }

    private void Filter()
    {
        var words = ItemSearchBox.Normalize(Search.Text ?? string.Empty).Split(' ', StringSplitOptions.RemoveEmptyEntries);
        IEnumerable<Entry> matches = GetEntries();
        if (words.Length != 0)
            matches = matches.Where(z => words.All(w => z.Key.Contains(w, StringComparison.Ordinal)));
        Results.ItemsSource = matches.ToArray();
    }

    /// <summary>
    /// One entry per name: trees and bushes first, then flowers, fences, rocks and the rest.
    /// </summary>
    public static Entry[] GetEntries()
    {
        if (CachedLanguage == GameInfo.CurrentLanguage)
            return CachedEntries;
        CachedLanguage = GameInfo.CurrentLanguage;

        var seen = new HashSet<string>();
        var list = new List<Entry>();
        foreach (var def in FieldItemList.Items.Values.OrderBy(z => z.Index))
        {
            var name = GridItems.GetFieldName(def);
            if (!seen.Add(name))
                continue;
            var k = def.Kind;
            var group = k.IsTree ? 0 : k.IsBush ? 1 : k.IsFlower ? 2 : k.IsFence ? 3 : k.IsStone ? 4 : 5;
            list.Add(new Entry((ushort)def.Index, name, ItemSearchBox.Normalize(name), group));
        }
        return CachedEntries = [.. list.OrderBy(z => z.Group).ThenBy(z => z.Key, StringComparer.Ordinal)];
    }
}
