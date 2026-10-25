using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Data;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Friendly event flags (of a player or of the island): what the game remembers of what already happened, each one
/// explained in words, with a tick for the yes/no ones and a number for the counters. A search box and a filter find them.
/// Edits the values in place; closing without saving puts the old ones back.
/// </summary>
public sealed class EventFlagsWindow : PlayerDialog
{
    private readonly short[] Values;
    private readonly short[] Original;
    private readonly FlagRow[] All;
    private readonly ListBox List;
    private readonly TextBlock Summary = PlayerUi.Subtle(string.Empty, 12);
    private string[] Words = [];
    private int Filter;

    internal IReadOnlyList<FlagRow> Shown => (IReadOnlyList<FlagRow>?)List.ItemsSource ?? [];

    /// <param name="values">The flags (edited in place).</param>
    /// <param name="island">True for the island flags, false for a player's.</param>
    /// <param name="title">Window title.</param>
    public EventFlagsWindow(short[] values, bool island, string title) : base(title, 900, 720)
    {
        Values = values;
        Original = [.. values];
        IEnumerable<(ushort Index, short Max, string Name)> known = island
            ? EventFlagLand.List.Select(z => (z.Key, z.Value.MaxValue, z.Value.Name))
            : EventFlagPlayer.List.Select(z => (z.Key, z.Value.MaxValue, z.Value.Name));
        All = [.. known.Where(z => z.Index < values.Length).OrderBy(z => z.Index)
            .Select(z => new FlagRow(this, z.Index, z.Max, z.Name, FlagDescriptions.Get(island, z.Index) ?? PlayerUi.SplitWords(z.Name)))];

        List = new ListBox
        {
            ItemTemplate = new FuncDataTemplate<FlagRow>((_, _) => RowView()),
            Background = Brushes.Transparent,
        };

        var search = PlayerUi.SearchBox(FlagStrings.Search, w =>
        {
            Words = w;
            Refilter();
        });
        var chips = PlayerUi.Chips([FlagStrings.FilterAll, FlagStrings.FilterOn, FlagStrings.FilterOff, FlagStrings.FilterChanged], 0, i =>
        {
            Filter = i;
            Refilter();
        });
        var top = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto"), ColumnSpacing = 12, Margin = new Thickness(0, 0, 0, 8) };
        top.Children.Add(search);
        Grid.SetColumn(chips, 1);
        top.Children.Add(chips);

        var warning = new Border { Classes = { "card" }, Padding = new Thickness(12, 8), Margin = new Thickness(0, 0, 0, 8) };
        warning.Child = new TextBlock { Text = FlagStrings.Warning, TextWrapping = TextWrapping.Wrap, FontSize = 12 };

        var body = new DockPanel();
        DockPanel.SetDock(top, Dock.Top);
        DockPanel.SetDock(warning, Dock.Top);
        DockPanel.SetDock(Summary, Dock.Bottom);
        Summary.Margin = new Thickness(2, 6, 0, 0);
        body.Children.Add(top);
        body.Children.Add(warning);
        body.Children.Add(Summary);
        body.Children.Add(List);

        var more = PlayerUi.MenuButton(HomeStrings.More,
            (FlagStrings.Undo, UndoAll),
            (FlagStrings.Export, () => _ = Export(island)),
            (FlagStrings.Import, () => _ = Import(island)));
        SetLayout(title, island ? FlagStrings.IslandHint : FlagStrings.PlayerHint, body, more);
        Refilter();
    }

    protected override void Save()
    {
        // The values were changed in place.
    }

    protected override void Discard() => Original.CopyTo(Values, 0);

    internal short GetValue(ushort index) => Values[index];

    internal void SetValue(ushort index, short value)
    {
        Values[index] = value;
        UpdateSummary();
    }

    /// <summary>
    /// Shows the flags matching the typed words (in the explanation or the internal name) and the chosen filter.
    /// </summary>
    public void Search(string text)
    {
        Words = PlayerUi.Words(text);
        Refilter();
    }

    private void Refilter()
    {
        var list = All.Where(r => PlayerUi.Matches(r.Key, Words) && Filter switch
        {
            1 => r.Value != 0,
            2 => r.Value == 0,
            3 => r.Value != Original[r.Index],
            _ => true,
        }).ToList();
        List.ItemsSource = list;
        UpdateSummary();
    }

    private void UpdateSummary()
    {
        var changed = All.Count(r => r.Value != Original[r.Index]);
        Summary.Text = string.Format(FlagStrings.Summary, Shown.Count.ToString("N0"), All.Length.ToString("N0"), changed);
    }

    private void UndoAll()
    {
        Original.CopyTo(Values, 0);
        foreach (var r in All)
            r.NotifyAll();
        Refilter();
    }

    private async Task Export(bool island)
    {
        var data = System.Runtime.InteropServices.MemoryMarshal.AsBytes(Values.AsSpan()).ToArray();
        await MiscDumpHelper.DumpFlags(this, data, island ? nameof(EventFlagLand) : nameof(EventFlagPlayer));
    }

    private async Task Import(bool island)
    {
        var data = await MiscDumpHelper.LoadFlags(this, Values.Length * 2, island ? nameof(EventFlagLand) : nameof(EventFlagPlayer));
        if (data.Length == 0)
            return;
        Buffer.BlockCopy(data, 0, Values, 0, data.Length);
        foreach (var r in All)
            r.NotifyAll();
        Refilter();
    }

    private static Control RowView()
    {
        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("*,130"), ColumnSpacing = 14, Margin = new Thickness(2, 3) };
        var text = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Spacing = 1 };
        var description = new TextBlock { FontSize = 14, TextWrapping = TextWrapping.Wrap };
        description.Bind(TextBlock.TextProperty, new Binding(nameof(FlagRow.Description)));
        var name = PlayerUi.Subtle(string.Empty, 11);
        name.Bind(TextBlock.TextProperty, new Binding(nameof(FlagRow.Technical)));
        text.Children.Add(description);
        text.Children.Add(name);
        g.Children.Add(text);

        var box = new CheckBox { Content = FlagStrings.Yes, VerticalAlignment = VerticalAlignment.Center };
        box.Bind(ToggleButton.IsCheckedProperty, new Binding(nameof(FlagRow.IsOn)) { Mode = BindingMode.TwoWay });
        box.Bind(IsVisibleProperty, new Binding(nameof(FlagRow.IsYesNo)));
        var number = new NumericUpDown { Minimum = 0, FormatString = "0", Width = 120, VerticalAlignment = VerticalAlignment.Center, HorizontalAlignment = HorizontalAlignment.Left };
        number.Bind(NumericUpDown.MaximumProperty, new Binding(nameof(FlagRow.Maximum)));
        number.Bind(NumericUpDown.ValueProperty, new Binding(nameof(FlagRow.Number)) { Mode = BindingMode.TwoWay });
        number.Bind(IsVisibleProperty, new Binding(nameof(FlagRow.IsCounter)));
        var right = new Panel();
        right.Children.Add(box);
        right.Children.Add(number);
        Grid.SetColumn(right, 1);
        g.Children.Add(right);
        return g;
    }
}

/// <summary>
/// One event flag: its explanation, whether it is a yes/no one or a counter, and its value in the window.
/// </summary>
public sealed class FlagRow(EventFlagsWindow owner, ushort index, short max, string name, string description) : RowModel
{
    public ushort Index { get; } = index;
    public string Description { get; } = description;
    public string Technical { get; } = $"#{index} · {name}";
    public string Key { get; } = ItemSearchBox.Normalize($"{description} {name}");

    public bool IsYesNo => max == 1;
    public bool IsCounter => !IsYesNo;
    public decimal Maximum => max > 1 ? max : short.MaxValue;

    public short Value => owner.GetValue(Index);

    public bool IsOn
    {
        get => Value != 0;
        set
        {
            if (value == IsOn)
                return;
            owner.SetValue(Index, (short)(value ? 1 : 0));
            NotifyAll();
        }
    }

    public decimal? Number
    {
        get => Value;
        set
        {
            var v = (short)Math.Clamp(value ?? 0, short.MinValue, short.MaxValue);
            if (v == Value)
                return;
            owner.SetValue(Index, v);
            NotifyAll();
        }
    }
}

/// <summary>
/// Explanations of the event flags in the game language (<c>Resources/flags_xx.tsv</c>: kind, index, text).
/// </summary>
public static class FlagDescriptions
{
    private static string? Language;
    private static Dictionary<(bool Island, int Index), string> Texts = [];

    public static string? Get(bool island, int index)
    {
        var lang = GameInfo.CurrentLanguage;
        if (Language != lang)
        {
            Texts = Load(lang);
            Language = lang;
        }
        return Texts.GetValueOrDefault((island, index));
    }

    private static Dictionary<(bool, int), string> Load(string lang)
    {
        var result = new Dictionary<(bool, int), string>();
        var text = EmbeddedResources.GetText($"mac/flags_{lang}.tsv");
        if (text is null)
            return result;
        foreach (var line in text.Split('\n'))
        {
            var parts = line.TrimEnd('\r').Split('\t');
            if (parts.Length < 3 || !int.TryParse(parts[1], out var index))
                continue;
            result[(parts[0] == "land", index)] = parts[2];
        }
        return result;
    }
}
