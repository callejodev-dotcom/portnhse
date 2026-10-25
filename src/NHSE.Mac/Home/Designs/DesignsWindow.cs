using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform.Storage;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Friendly designs window: every design as a picture, the chosen one large with its colours, name and maker,
/// and plain buttons to save it as a picture, save it to a file or bring one from a file (also by dragging files in).
/// Works on copies; «Save» writes them back through the function given by the page.
/// </summary>
public sealed class DesignsWindow : PlayerDialog
{
    private readonly DesignSlot[] Slots;
    private readonly DesignKind Kind;
    private readonly IVillagerOrigin Player;
    private readonly Action<DesignSlot[]> WriteBack;

    private readonly ListBox List = new() { Classes = { "designs" } };
    private readonly ContentControl Preview = new();
    private readonly WrapPanel Palette = new() { ItemSpacing = 4, LineSpacing = 4 };
    private readonly TextBox NameBox = new() { MaxLength = 20, Width = 240, HorizontalAlignment = HorizontalAlignment.Left };
    private readonly TextBlock MadeBy = new() { Classes = { "subtle" }, TextWrapping = TextWrapping.Wrap };
    private readonly Border NotMine = new() { Classes = { "card" }, Padding = new Thickness(12, 10) };
    private readonly StackPanel Details = new() { Spacing = 10 };
    private readonly TextBlock EmptyText = new() { Text = DesignStrings.EmptySlot, TextWrapping = TextWrapping.Wrap, Classes = { "subtle" } };
    private bool Loading;

    public int Index { get; private set; }

    public DesignsWindow(string title, DesignKind kind, IReadOnlyList<DesignSlot> designs, IVillagerOrigin player, Action<DesignSlot[]> writeBack)
        : base(title, designs.Count == 1 ? 620 : 920, designs.Count == 1 ? 560 : 680)
    {
        Slots = [.. designs];
        Kind = kind;
        Player = player;
        WriteBack = writeBack;

        NameBox.TextChanged += (_, _) =>
        {
            if (Loading)
                return;
            Slots[Index].Name = NameBox.Text ?? string.Empty;
            RefreshTile(Index);
        };

        var makeMine = new Button { Content = DesignStrings.MakeMine, Margin = new Thickness(0, 8, 0, 0) };
        makeMine.Click += (_, _) => MakeMine();
        var notMine = new StackPanel();
        notMine.Children.Add(new TextBlock { Text = DesignStrings.NotMine, TextWrapping = TextWrapping.Wrap });
        notMine.Children.Add(makeMine);
        NotMine.Child = notMine;

        var buttons = new WrapPanel { ItemSpacing = 6, LineSpacing = 6 };
        buttons.Children.Add(PlayerUi.Button(DesignStrings.SavePicture, () => _ = SavePicture()));
        buttons.Children.Add(PlayerUi.Button(DesignStrings.Export, () => _ = Export()));
        buttons.Children.Add(PlayerUi.Button(DesignStrings.Import, () => _ = ImportFile()));

        var name = new StackPanel { Spacing = 4 };
        name.Children.Add(new TextBlock { Text = DesignStrings.Name, FontWeight = FontWeight.SemiBold });
        name.Children.Add(NameBox);
        name.Children.Add(MadeBy);

        var colors = new StackPanel { Spacing = 4 };
        colors.Children.Add(new TextBlock { Text = DesignStrings.Colors, FontWeight = FontWeight.SemiBold });
        colors.Children.Add(Palette);

        Details.Children.Add(name);
        Details.Children.Add(colors);
        Details.Children.Add(NotMine);

        var side = new StackPanel { Spacing = 14 };
        side.Children.Add(Preview);
        side.Children.Add(EmptyText);
        side.Children.Add(Details);
        side.Children.Add(buttons);
        var card = new Border { Classes = { "card" }, Child = new ScrollViewer { Content = side }, VerticalAlignment = VerticalAlignment.Stretch };

        Control body;
        Control[] tools;
        if (Slots.Length == 1)
        {
            body = card;
            tools = [];
        }
        else
        {
            List.ItemsPanel = new FuncTemplate<Panel?>(() => new WrapPanel());
            List.ItemsSource = Slots.Select(Tile).ToArray();
            List.SelectionChanged += (_, _) =>
            {
                if (List.SelectedIndex >= 0)
                    Select(List.SelectedIndex);
            };
            var left = new DockPanel();
            var hint = PlayerUi.Subtle(string.Format(DesignStrings.DropHint, Kind.Extension));
            hint.Margin = new Thickness(2, 8, 0, 0);
            DockPanel.SetDock(hint, Dock.Bottom);
            left.Children.Add(hint);
            left.Children.Add(List);

            var g = new Grid { ColumnDefinitions = new ColumnDefinitions("*,340"), ColumnSpacing = 16 };
            g.Children.Add(left);
            Grid.SetColumn(card, 1);
            g.Children.Add(card);
            body = g;
            tools = [PlayerUi.MenuButton(HomeStrings.More,
                (DesignStrings.ExportAll, () => _ = ExportAll()),
                (DesignStrings.ImportFolder, () => _ = ImportFolder()))];
        }

        SetLayout(title, Slots.Length == 1 ? DesignStrings.FlagWindowHint : DesignStrings.WindowHint, body, tools);

        DragDrop.SetAllowDrop(this, true);
        AddHandler(DragDrop.DragOverEvent, (_, e) =>
        {
            e.DragEffects = e.DataTransfer.TryGetFiles()?.Length > 0 ? DragDropEffects.Copy : DragDropEffects.None;
        });
        AddHandler(DragDrop.DropEvent, (_, e) =>
        {
            var files = e.DataTransfer.TryGetFiles()?.Select(z => z.TryGetLocalPath()).OfType<string>().ToArray();
            if (files is { Length: > 0 })
                _ = Import(files);
        });

        Select(FirstUsed());
    }

    /// <summary>
    /// Normal designs, from <see cref="MainSave.GetDesigns"/> or a single one (the flag).
    /// </summary>
    /// <param name="saved">Writes <paramref name="patterns"/> (already updated) back to the save.</param>
    public static DesignsWindow Normal(string title, DesignPattern[] patterns, IVillagerOrigin player, Action saved)
        => new(title, DesignKind.Normal, [.. patterns.Select(z => new NormalDesign(z))], player, slots =>
        {
            for (int i = 0; i < slots.Length; i++)
                patterns[i] = ((NormalDesign)slots[i]).Pattern;
            saved();
        });

    /// <summary>
    /// PRO designs, from <see cref="MainSave.GetDesignsPRO"/> or the tailor's.
    /// </summary>
    /// <param name="saved">Writes <paramref name="patterns"/> (already updated) back to the save.</param>
    public static DesignsWindow Pro(string title, DesignPatternPRO[] patterns, IVillagerOrigin player, Action saved)
        => new(title, DesignKind.Pro, [.. patterns.Select(z => new ProDesign(z))], player, slots =>
        {
            for (int i = 0; i < slots.Length; i++)
                patterns[i] = ((ProDesign)slots[i]).Pattern;
            saved();
        });

    internal IReadOnlyList<DesignSlot> Designs => Slots;

    protected override void Save() => WriteBack(Slots);

    private int FirstUsed()
    {
        var i = Array.FindIndex(Slots, z => !z.IsEmpty);
        return i < 0 ? 0 : i;
    }

    public void Select(int index)
    {
        Index = index;
        if (List.SelectedIndex != index && Slots.Length > 1)
            List.SelectedIndex = index;

        var d = Slots[index];
        var empty = d.IsEmpty;
        Preview.Content = Picture(d, d.SheetCount == 1 ? 6 : 3, 192);
        EmptyText.IsVisible = empty;
        Details.IsVisible = !empty;

        Loading = true;
        NameBox.Text = d.Name;
        Loading = false;
        MadeBy.Text = d.IsFromGame ? DesignStrings.FromGame : string.Format(DesignStrings.MadeBy, d.Maker, d.Town);
        NotMine.IsVisible = !empty && !d.IsFromGame && !d.IsMadeBy(Player);

        Palette.Children.Clear();
        foreach (var argb in d.GetColors())
        {
            var dot = new Border
            {
                Width = 18,
                Height = 18,
                CornerRadius = new CornerRadius(9),
                Background = new SolidColorBrush(Color.FromUInt32((uint)argb)),
                BorderThickness = new Thickness(1),
            };
            dot.Bind(Border.BorderBrushProperty, dot.GetResourceObservable("NhCardBorderBrush"));
            Palette.Children.Add(dot);
        }
    }

    /// <summary>
    /// The design on a soft square, so see-through pixels show as the background.
    /// </summary>
    private static Border Picture(DesignSlot d, int scale, double size) => new Border
    {
        Width = size + 8,
        Height = size + 8,
        Padding = new Thickness(4),
        CornerRadius = new CornerRadius(8),
        HorizontalAlignment = HorizontalAlignment.Left,
        Child = new Image { Source = d.GetPicture(scale), Width = size, Height = size },
    }.WithResource(Border.BackgroundProperty, "NhSidebarBrush");

    private static Control Tile(DesignSlot d)
    {
        var p = new StackPanel { Width = 78, Spacing = 3 };
        if (d.IsEmpty)
        {
            var blank = new Border { Width = 64, Height = 64, CornerRadius = new CornerRadius(6), BorderThickness = new Thickness(1.5) };
            blank.Bind(Border.BorderBrushProperty, blank.GetResourceObservable("NhCardBorderBrush"));
            p.Children.Add(blank);
        }
        else
        {
            p.Children.Add(new Image { Source = d.GetPicture(d.SheetCount == 1 ? 2 : 1), Width = 64, Height = 64 });
        }
        var label = new TextBlock { FontSize = 11, TextAlignment = TextAlignment.Center, TextTrimming = TextTrimming.CharacterEllipsis };
        if (d.IsEmpty)
            label.Classes.Add("subtle");
        p.Children.Add(label);
        SetLabel(p, d);
        return p;
    }

    private static void SetLabel(StackPanel tile, DesignSlot d)
    {
        var text = d.IsEmpty ? DesignStrings.Empty : d.Name.Length == 0 ? DesignStrings.NoName : d.Name;
        ((TextBlock)tile.Children[1]).Text = text;
        ToolTip.SetTip(tile, text);
    }

    private void RefreshTile(int index)
    {
        if (List.ItemsSource is Control[] tiles)
            SetLabel((StackPanel)tiles[index], Slots[index]);
    }

    private void MakeMine()
    {
        Slots[Index].MakeMine(Player);
        Select(Index);
        Sound.Asterisk();
    }

    private string FileName(DesignSlot d) => StringUtil.CleanFileName((d.Name.Length == 0 ? DesignStrings.NoName : d.Name) + Kind.Extension);

    private async Task SavePicture()
    {
        var d = Slots[Index];
        var path = await Pickers.SaveFile(this, "PNG (*.png)|*.png", Path.ChangeExtension(FileName(d), ".png"));
        if (path is null)
            return;
        using var bmp = d.GetPicture(d.SheetCount == 1 ? 8 : 4);
        bmp.Save(path, new PngBitmapEncoderOptions());
    }

    private async Task Export()
    {
        var d = Slots[Index];
        var path = await Pickers.SaveFile(this, Kind.Filter, FileName(d));
        if (path is null)
            return;
        await File.WriteAllBytesAsync(path, d.Data.ToArray());
    }

    private async Task ExportAll()
    {
        var folder = await Pickers.OpenFolder(this);
        if (folder is null || !Directory.Exists(folder))
            return;
        int count = 0;
        for (int i = 0; i < Slots.Length; i++)
        {
            var d = Slots[i];
            if (d.IsEmpty)
                continue;
            var file = StringUtil.CleanFileName($"{i + 1:00} - {FileName(d)}");
            await File.WriteAllBytesAsync(Path.Combine(folder, file), d.Data.ToArray());
            count++;
        }
        await Dialogs.Alert(string.Format(DesignStrings.Saved, count));
    }

    private async Task ImportFile()
    {
        var path = await Pickers.OpenFile(this, Kind.Filter);
        if (path is not null)
            await Import([path]);
    }

    private async Task ImportFolder()
    {
        var folder = await Pickers.OpenFolder(this);
        if (folder is null || !Directory.Exists(folder))
            return;
        var files = Directory.GetFiles(folder, "*" + Kind.Extension, SearchOption.TopDirectoryOnly)
            .Order(StringComparer.OrdinalIgnoreCase).ToArray();
        if (files.Length == 0)
        {
            await Dialogs.Alert(string.Format(DesignStrings.NoneInFolder, Kind.Extension));
            return;
        }
        await Import(files);
    }

    /// <summary>
    /// Puts the designs in the files into the chosen slot and the ones after it.
    /// </summary>
    /// <returns>How many were brought in.</returns>
    public async Task<int> Import(IReadOnlyList<string> files)
    {
        var read = new List<DesignSlot>();
        foreach (var f in files.Take(Slots.Length - Index))
        {
            if (!Kind.CanRead(f))
            {
                await Dialogs.Error(string.Format(DesignStrings.WrongFile, Path.GetFileName(f), Kind.Extension));
                return 0;
            }
            read.Add(Kind.Read(await File.ReadAllBytesAsync(f)));
        }
        if (read.Count == 0)
            return 0;

        var others = read.Where(z => !z.IsFromGame && !z.IsMadeBy(Player)).ToArray();
        if (others.Length != 0)
        {
            var ask = others.Length == 1 && read.Count == 1
                ? string.Format(DesignStrings.AskMakeMine, others[0].Maker)
                : DesignStrings.AskMakeMineMany;
            var answer = await Dialogs.Prompt(MessageBoxButtons.YesNoCancel, ask);
            if (answer == DialogResult.Cancel)
                return 0;
            if (answer == DialogResult.Yes)
            {
                foreach (var d in others)
                    d.MakeMine(Player);
            }
        }

        for (int i = 0; i < read.Count; i++)
        {
            read[i].FixUsage();
            Slots[Index + i] = read[i];
        }
        if (Slots.Length > 1)
            List.ItemsSource = Slots.Select(Tile).ToArray();
        Select(Index);
        if (read.Count > 1)
            await Dialogs.Alert(string.Format(DesignStrings.Loaded, read.Count));
        return read.Count;
    }
}

internal static class ResourceExtensions
{
    /// <summary>
    /// Binds a property to a theme resource (so it follows light/dark), in an object initializer.
    /// </summary>
    public static T WithResource<T>(this T control, AvaloniaProperty property, string key) where T : Control
    {
        control.Bind(property, control.GetResourceObservable(key));
        return control;
    }
}
