using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Friendly editor of the native fruit and flowers of the island and of the sister island: pictures to click
/// instead of lists. Saves like the classic editor (island fruit, passport fruit, fruit flags and flowers).
/// </summary>
public sealed class FruitsFlowersWindow : NhseWindow
{
    private const ushort Apple = 2213, Orange = 2214, Pear = 2285, Peach = 2286, Cherry = 2287;

    /// <summary>
    /// Fruit in the order of the game's fruit flags.
    /// </summary>
    private static readonly ushort[] FruitOrder = [Apple, Orange, Pear, Peach, Cherry];

    private readonly IReadOnlyList<Player> Players;
    private readonly int PlayerIndex;
    private readonly MainSave Main;

    internal TilePicker<ushort> NativeFruit { get; }
    internal TilePicker<ushort> SecondFruit { get; }
    internal TilePicker<IslandFlowers> NativeFlower { get; }
    internal TilePicker<IslandFlowers> SecondFlower { get; }

    public FruitsFlowersWindow(IReadOnlyList<Player> players, int playerIndex, MainSave main)
    {
        Players = players;
        PlayerIndex = playerIndex;
        Main = main;
        SizeToContent = SizeToContent.WidthAndHeight;
        CanResize = false;

        var fruits = FruitOrder.Select(z => (z, IslandUi.ItemName(z), ItemSprite.GetItemSprite(z))).ToArray();
        var native = IsFruit(main.SpecialtyFruit) ? main.SpecialtyFruit : players[playerIndex].Personal.ProfileFruit;
        NativeFruit = new TilePicker<ushort>(fruits, native);
        SecondFruit = new TilePicker<ushort>(fruits, main.SisterFruit);

        var flowers = Enum.GetValues<IslandFlowers>().Select(z => (z, FlowerName(z), FlowerIcon(z))).ToArray();
        NativeFlower = new TilePicker<IslandFlowers>(flowers, main.SpecialtyFlower);
        SecondFlower = new TilePicker<IslandFlowers>(flowers, main.SisterFlower);

        var fruitCard = Card(CardHeader("Peach", IslandStrings.Fruit, null,
            Section(IslandStrings.NativeFruit, IslandStrings.NativeFruitHint, NativeFruit, true),
            Section(IslandStrings.SecondFruit, IslandStrings.SecondFruitHint, SecondFruit, false)));
        var flowerCard = Card(CardHeader("PltCosmos0", IslandStrings.Flowers, null,
            Section(IslandStrings.NativeFlower, IslandStrings.NativeFlowerHint, NativeFlower, true),
            Section(IslandStrings.SecondFlower, IslandStrings.SecondFlowerHint, SecondFlower, false)));

        // A fruit that is not one of the five, by item number (the classic editor's "custom" value)
        var advanced = new StackPanel { Spacing = 6 };
        advanced.Children.Add(Text(IslandStrings.FruitAdvancedHint, "subtle"));
        advanced.Children.Add(IslandUi.Labeled(IslandStrings.NativeFruitNumber, 150, NumberBox(NativeFruit)));
        advanced.Children.Add(IslandUi.Labeled(IslandStrings.SecondFruitNumber, 150, NumberBox(SecondFruit)));
        var expander = new Expander { Header = HomeStrings.AdvancedOptions, Content = advanced, HorizontalAlignment = HorizontalAlignment.Stretch };

        var body = new StackPanel { Spacing = 12, Width = 790 };
        body.Children.Add(fruitCard);
        body.Children.Add(flowerCard);
        body.Children.Add(expander);

        IslandUi.Frame(this, IslandStrings.FruitTitle, IslandStrings.FruitHint, body, SaveFruitsFlowers);
    }

    private static Control Section(string title, string hint, Control picker, bool first)
    {
        var p = new StackPanel { Margin = new Thickness(0, first ? 10 : 16, 0, 0) };
        p.Children.Add(new TextBlock { Text = title, FontWeight = FontWeight.SemiBold });
        var caption = Text(hint, "subtle");
        caption.FontSize = 12;
        caption.Margin = new Thickness(0, 1, 0, 6);
        p.Children.Add(caption);
        p.Children.Add(picker);
        return p;
    }

    /// <summary>
    /// Item number of a fruit, kept in step with its pictures.
    /// </summary>
    private static NumericUpDown NumberBox(TilePicker<ushort> picker)
    {
        var box = new NumericUpDown { Minimum = 0, Maximum = ushort.MaxValue, Value = picker.Value, FormatString = "0", Width = 120 };
        box.ValueChanged += (_, _) =>
        {
            var v = (ushort)(box.Value ?? 0);
            if (picker.Value != v)
                picker.Value = v;
        };
        picker.ValueChanged += v => box.Value = v;
        return box;
    }

    private static bool IsFruit(ushort id) => Array.IndexOf(FruitOrder, id) >= 0;

    internal static string FlowerName(IslandFlowers f) => f switch
    {
        IslandFlowers.Windflower => IslandStrings.FlowerWindflower,
        IslandFlowers.Hyacinth => IslandStrings.FlowerHyacinth,
        IslandFlowers.Tulip => IslandStrings.FlowerTulip,
        IslandFlowers.Rose => IslandStrings.FlowerRose,
        IslandFlowers.Cosmos => IslandStrings.FlowerCosmos,
        IslandFlowers.Lily => IslandStrings.FlowerLily,
        IslandFlowers.Mums => IslandStrings.FlowerMum,
        _ => IslandStrings.FlowerPansy,
    };

    internal static Bitmap? FlowerIcon(IslandFlowers f) => ItemSprite.GetResource(f switch
    {
        IslandFlowers.Windflower => "sprites/MenuIcon/Anemone1.png",
        IslandFlowers.Hyacinth => "sprites/MenuIcon/Hyacinth5.png",
        IslandFlowers.Tulip => "sprites/MenuIcon/Turip0.png",
        IslandFlowers.Rose => "sprites/MenuIcon/Rose0.png",
        IslandFlowers.Cosmos => "sprites/MenuIcon/Cosmos2.png",
        IslandFlowers.Lily => "sprites/MenuIcon/Yuri2.png",
        IslandFlowers.Mums => "sprites/MenuIcon/Mum1.png",
        _ => "sprites/MenuIcon/Pansi1.png",
    });

    private void SaveFruitsFlowers()
    {
        var oldNative = Main.SpecialtyFruit;
        var native = NativeFruit.Value;

        // The island's fruit is on every passport: change it for the players that had the old one, and for the current one.
        for (int i = 0; i < Players.Count; i++)
        {
            var pers = Players[i].Personal;
            if (i == PlayerIndex || pers.ProfileFruit == oldNative)
                pers.ProfileFruit = native;
        }
        Main.SpecialtyFruit = native;
        Main.SisterFruit = SecondFruit.Value;
        var flags = new byte[FruitOrder.Length];
        var index = Array.IndexOf(FruitOrder, native);
        if (index >= 0)
            flags[index] = 1;
        Main.FruitFlags = flags;

        Main.SpecialtyFlower = NativeFlower.Value;
        Main.SisterFlower = SecondFlower.Value;
    }
}

/// <summary>
/// Row of big picture buttons; the chosen one is marked with a green border (none if the value is not one of them).
/// </summary>
internal sealed class TilePicker<T> : WrapPanel where T : struct
{
    private readonly List<(T Value, Button Tile)> Tiles = [];
    private T _value;

    /// <summary>
    /// Raised when the value changes.
    /// </summary>
    public event Action<T>? ValueChanged;

    public T Value
    {
        get => _value;
        set
        {
            var changed = !EqualityComparer<T>.Default.Equals(_value, value);
            _value = value;
            foreach (var (v, tile) in Tiles)
                Mark(tile, EqualityComparer<T>.Default.Equals(v, value));
            if (changed)
                ValueChanged?.Invoke(value);
        }
    }

    public TilePicker(IEnumerable<(T Value, string Name, Bitmap? Picture)> choices, T current)
    {
        ItemSpacing = 6;
        LineSpacing = 6;
        foreach (var (value, name, picture) in choices)
        {
            var content = new StackPanel { Spacing = 2, HorizontalAlignment = HorizontalAlignment.Center };
            content.Children.Add(new Image { Source = picture, Width = 40, Height = 40, HorizontalAlignment = HorizontalAlignment.Center });
            content.Children.Add(new TextBlock { Text = name, FontSize = 12, HorizontalAlignment = HorizontalAlignment.Center, TextTrimming = TextTrimming.CharacterEllipsis });
            var tile = new Button
            {
                Content = content,
                Classes = { "card" },
                Width = 80,
                Padding = new Thickness(4, 8, 4, 6),
                HorizontalAlignment = HorizontalAlignment.Left,
                HorizontalContentAlignment = HorizontalAlignment.Center,
            };
            ToolTip.SetTip(tile, name);
            tile.Click += (_, _) => Value = value;
            Tiles.Add((value, tile));
            Children.Add(tile);
        }
        Value = current;
    }

    private static void Mark(Button tile, bool chosen)
    {
        tile.BorderThickness = new Thickness(chosen ? 2 : 1);
        tile.Themed(Button.BorderBrushProperty, chosen ? "NhGreenBrush" : "NhCardBorderBrush");
        tile.Themed(Button.BackgroundProperty, chosen ? "NhSelectedBrush" : "NhCardBrush");
    }
}
