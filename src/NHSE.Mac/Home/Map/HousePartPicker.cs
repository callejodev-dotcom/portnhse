using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Chooses a look of a house (roof, walls or door) by its picture: a button with the current one that opens a grid
/// with all of them, each with its picture and its name in the game. Pictures still downloading appear when ready.
/// </summary>
public sealed class HousePartPicker<T> : UserControl where T : struct, Enum
{
    private readonly IReadOnlyList<T> Values;
    private readonly Button Current = new()
    {
        HorizontalAlignment = HorizontalAlignment.Stretch,
        HorizontalContentAlignment = HorizontalAlignment.Stretch,
        Padding = new Thickness(6, 4),
    };
    private readonly Flyout Choices = new() { Placement = PlacementMode.BottomEdgeAlignedLeft };

    public T Value { get; private set; }

    /// <summary> Raised when another look is chosen. </summary>
    public event Action<T>? Changed;

    /// <param name="values">Looks to offer (the current one is added if missing).</param>
    /// <param name="current">Look the house has now.</param>
    public HousePartPicker(IEnumerable<T> values, T current)
    {
        var list = values.ToList();
        if (!list.Contains(current))
            list.Insert(0, current);
        Values = list;
        Value = current;
        Current.Flyout = Choices;
        Choices.Opening += (_, _) => Choices.Content = Choice();
        Content = Current;
        Refresh();
    }

    public void SetValue(T value)
    {
        Value = value;
        Refresh();
    }

    /// <summary> Name of a look in the game, or one made from its internal name (walls have none in the game list). </summary>
    public static string NameOf(T value) => ExteriorPictures.GetName(value.ToString()) ?? HouseStyleNames.GetName(value);

    /// <summary> The look's picture, or a plain square with its first letters while there is none. </summary>
    public static Control Picture(T value, double size)
    {
        if (ExteriorPictures.Get(value.ToString()) is { } bmp)
            return new Image { Source = bmp, Width = size, Height = size };
        var empty = new Border { Width = size, Height = size, CornerRadius = new CornerRadius(size * 0.15) };
        empty.Bind(Border.BackgroundProperty, empty.GetResourceObservable("NhSelectedBrush"));
        return empty;
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnAttachedToVisualTree(e);
        ExteriorPictures.PicturesChanged += Refresh;
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnDetachedFromVisualTree(e);
        ExteriorPictures.PicturesChanged -= Refresh;
    }

    private void Refresh()
    {
        var g = new Avalonia.Controls.Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto"), ColumnSpacing = 10 };
        g.Children.Add(Picture(Value, 46));
        var name = new TextBlock { Text = NameOf(Value), VerticalAlignment = VerticalAlignment.Center, HorizontalAlignment = HorizontalAlignment.Left, TextAlignment = TextAlignment.Left, TextWrapping = TextWrapping.Wrap };
        Avalonia.Controls.Grid.SetColumn(name, 1);
        g.Children.Add(name);
        var arrow = new TextBlock { Text = "▾", VerticalAlignment = VerticalAlignment.Center, Classes = { "subtle" } };
        Avalonia.Controls.Grid.SetColumn(arrow, 2);
        g.Children.Add(arrow);
        Current.Content = g;
    }

    internal Control Choice()
    {
        var wrap = new WrapPanel { ItemSpacing = 6, LineSpacing = 6 };
        foreach (var v in Values)
        {
            var value = v;
            var content = new StackPanel { Spacing = 4, Width = 96 };
            var picture = Picture(value, 84);
            picture.HorizontalAlignment = HorizontalAlignment.Center;
            content.Children.Add(picture);
            content.Children.Add(new TextBlock
            {
                Text = NameOf(value),
                FontSize = 11,
                TextWrapping = TextWrapping.Wrap,
                TextAlignment = TextAlignment.Center,
                MaxLines = 2,
                TextTrimming = TextTrimming.CharacterEllipsis,
            });
            var tile = new Button { Content = content, Padding = new Thickness(4, 6), Classes = { "parttile" } };
            if (EqualityComparer<T>.Default.Equals(value, Value))
                tile.Classes.Add("chosen");
            ToolTip.SetTip(tile, NameOf(value));
            tile.Click += (_, _) =>
            {
                Choices.Hide();
                if (EqualityComparer<T>.Default.Equals(value, Value))
                    return;
                Value = value;
                Refresh();
                Changed?.Invoke(value);
            };
            wrap.Children.Add(tile);
        }
        return new ScrollViewer { Content = wrap, MaxHeight = 470, Width = 560, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled };
    }
}

/// <summary>
/// Chooses a door: its design by picture, and its shape (arched or square), which the game keeps as two looks of each design.
/// </summary>
public sealed class DoorPicker : UserControl
{
    internal readonly HousePartPicker<DoorKind> Design;
    private readonly Action<int> ShowShape;
    private bool Square;

    public DoorKind Value { get; private set; }

    public event Action<DoorKind>? Changed;

    public DoorPicker(DoorKind current)
    {
        Value = current;
        var name = current.ToString();
        Square = name.StartsWith("HouseDoor", StringComparison.Ordinal) && name.EndsWith('S');
        var designs = Enum.GetValues<DoorKind>().Where(z => z.ToString() is var n && n.StartsWith("HouseDoor", StringComparison.Ordinal) && n.EndsWith('R') && ExteriorPictures.Has(n));
        Design = new HousePartPicker<DoorKind>(designs, Arched(current));
        Design.Changed += _ => Update();

        var (shape, show) = TilePanel.Segmented([MapStrings.DoorArched, MapStrings.DoorSquare], Square ? 1 : 0, i =>
        {
            Square = i == 1;
            Update();
        });
        ShowShape = show;

        var p = new StackPanel { Spacing = 6 };
        p.Children.Add(Design);
        p.Children.Add(shape);
        Content = p;
    }

    /// <summary> The arched look of a door design (the one with a picture). </summary>
    private static DoorKind Arched(DoorKind door)
    {
        var name = door.ToString();
        if (name.StartsWith("HouseDoor", StringComparison.Ordinal) && name.EndsWith('S') && Enum.TryParse<DoorKind>(name[..^1] + "R", out var arched))
            return arched;
        return door;
    }

    private void Update()
    {
        var name = Design.Value.ToString();
        var wanted = Square && name.EndsWith('R') ? name[..^1] + "S" : name;
        if (!Enum.TryParse<DoorKind>(wanted, out var door))
        {
            door = Design.Value;
            Square = false;
            ShowShape(0);
        }
        if (door == Value)
            return;
        Value = door;
        Changed?.Invoke(door);
    }
}
