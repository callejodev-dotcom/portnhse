using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// What the side panel of the map and the house shows for a chosen spot, and what it can do there.
/// </summary>
public sealed class TilePanelOptions
{
    /// <summary> Item on the spot (its main tile), or an empty item. </summary>
    public required Item Item { get; init; }

    /// <summary> Where the spot is, in plain words ("On the ground"). </summary>
    public required string Where { get; init; }

    /// <summary> Something to tell after the last action (it didn't fit…). </summary>
    public string? Message { get; init; }

    /// <summary> Offer the planted things (trees, flowers, fences, rocks) as well as items. </summary>
    public bool AllowPlants { get; init; }

    /// <summary> Show only one category of items (walls and floors…). </summary>
    public ItemCategory? OnlyCategory { get; init; }

    /// <summary> Grid position, shown only in the advanced options. </summary>
    public (int X, int Y) Position { get; init; }

    /// <summary> Item list of the classic editor (with the planted things for the island). </summary>
    public IReadOnlyList<ComboItem>? ClassicItems { get; init; }

    public required Action<Item> Put { get; init; }
    public required Action Remove { get; init; }
    public Action? Rotate { get; init; }

    /// <summary> More buttons for the chosen thing (remove all of the same…). </summary>
    public IReadOnlyList<(string Text, Action Act)>? MoreActions { get; init; }
}

/// <summary>
/// Side panel of a chosen spot: its picture and name, remove and turn buttons, a search to put something else
/// (items, or plants and fences on the island) and the classic item editor in "Advanced options".
/// </summary>
public static class TilePanel
{
    /// <summary> Which picker was used last (items or plants), kept between spots. </summary>
    private static bool PlantsChosen;

    public static Control Build(TilePanelOptions o)
    {
        var item = o.Item;
        var panel = new StackPanel { Spacing = 10 };

        // Picture, name, where it is
        var picture = new Border
        {
            Width = 76,
            Height = 76,
            CornerRadius = new CornerRadius(14),
            Child = new Image { Source = GridItems.GetPicture(item) ?? GridItems.HomeIcon("Leaf"), Margin = new Thickness(8), Opacity = item.IsNone ? 0.25 : 1 },
        };
        picture.Bind(Border.BackgroundProperty, picture.GetResourceObservable("NhSelectedBrush"));
        var name = Text(item.IsNone ? MapStrings.EmptySpace : GridItems.GetName(item), "cardtitle");
        name.FontSize = 17;
        var nameBox = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Spacing = 2 };
        nameBox.Children.Add(name);
        var where = item.IsBuried ? $"{o.Where} · {MapStrings.Buried}" : o.Where;
        nameBox.Children.Add(Text(item.IsNone ? MapStrings.EmptySpaceHint : where, "subtle"));
        var top = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 12 };
        top.Children.Add(picture);
        Grid.SetColumn(nameBox, 1);
        top.Children.Add(nameBox);
        panel.Children.Add(top);

        if (!item.IsNone)
        {
            var actions = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
            var remove = new Button { Content = MapStrings.Remove, Padding = new Thickness(12, 4) };
            remove.Click += (_, _) => o.Remove();
            actions.Children.Add(remove);
            if (o.Rotate is { } rotate)
            {
                var turn = new Button { Content = "↻  " + MapStrings.Rotate, Padding = new Thickness(12, 4) };
                ToolTip.SetTip(turn, MapStrings.RotateHint);
                turn.Click += (_, _) => rotate();
                actions.Children.Add(turn);
            }
            panel.Children.Add(actions);
            if (o.MoreActions is { Count: > 0 } more)
            {
                var row = new WrapPanel { ItemSpacing = 8, LineSpacing = 6 };
                foreach (var (text, act) in more)
                {
                    var b = new Button { Content = text, Padding = new Thickness(10, 3), FontSize = 12 };
                    b.Click += (_, _) => act();
                    row.Children.Add(b);
                }
                panel.Children.Add(row);
            }
        }

        if (!string.IsNullOrEmpty(o.Message))
        {
            var msg = new Border
            {
                CornerRadius = new CornerRadius(8),
                Padding = new Thickness(10, 6),
                Background = new SolidColorBrush(Color.FromArgb(0x30, 0xE0, 0x6C, 0x3C)),
                Child = new TextBlock { Text = o.Message, TextWrapping = TextWrapping.Wrap },
            };
            panel.Children.Add(msg);
        }

        // Put something (else) here
        panel.Children.Add(new TextBlock { Text = item.IsNone ? MapStrings.PutHere : MapStrings.ChangeFor, FontWeight = FontWeight.SemiBold, Margin = new Thickness(0, 6, 0, 0) });
        var pickerHost = new ContentControl();
        Control ItemPicker()
        {
            var search = new ItemSearchBox(220, o.OnlyCategory);
            search.ItemChosen += (id, count) => o.Put(new Item(id) { Count = count });
            return search;
        }
        Control PlantPicker()
        {
            var plants = new FieldItemPicker(220);
            plants.ItemChosen += id => o.Put(new Item(id));
            return plants;
        }
        if (o.AllowPlants)
        {
            var (switcher, _) = Segmented([MapStrings.KindItems, MapStrings.KindPlants], PlantsChosen ? 1 : 0, i =>
            {
                PlantsChosen = i == 1;
                pickerHost.Content = PlantsChosen ? PlantPicker() : ItemPicker();
            });
            panel.Children.Add(switcher);
            pickerHost.Content = PlantsChosen ? PlantPicker() : ItemPicker();
        }
        else
        {
            pickerHost.Content = ItemPicker();
        }
        panel.Children.Add(pickerHost);

        // The classic editor, for those who know (built when opened: it loads every item name)
        var advanced = new StackPanel { Spacing = 6 };
        var expander = new Expander { Header = MapStrings.AdvancedOptions, Content = advanced, HorizontalAlignment = HorizontalAlignment.Stretch };
        expander.PropertyChanged += (_, e) =>
        {
            if (e.Property != Expander.IsExpandedProperty || !expander.IsExpanded || advanced.Children.Count != 0)
                return;
            advanced.Children.Add(Text(string.Format(MapStrings.Position, o.Position.X, o.Position.Y), "subtle"));
            var classic = new ItemEditor();
            classic.Initialize(o.ClassicItems ?? GameInfo.Strings.ItemDataSource, true);
            classic.LoadItem(item);
            var apply = new Button { Content = MapStrings.Apply };
            apply.Click += (_, _) => o.Put(classic.SetItem(new Item()));
            advanced.Children.Add(classic);
            advanced.Children.Add(apply);
        };
        panel.Children.Add(expander);
        return panel;
    }

    /// <summary>
    /// Row of options where one is chosen (a segmented control made of toggle buttons).
    /// </summary>
    /// <returns>The control, and a way to show another option as chosen from code (without raising <paramref name="changed"/>).</returns>
    public static (Control Control, Action<int> Show) Segmented(IReadOnlyList<string> options, int selected, Action<int> changed, IReadOnlyList<string>? tips = null)
    {
        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 2 };
        var box = new Border { Child = row, CornerRadius = new CornerRadius(9), Padding = new Thickness(2), HorizontalAlignment = HorizontalAlignment.Left };
        box.Bind(Border.BackgroundProperty, box.GetResourceObservable("NhSidebarBrush"));
        var buttons = new List<ToggleButton>();
        var current = -1;
        void Select(int index, bool notify)
        {
            for (int i = 0; i < buttons.Count; i++)
                buttons[i].IsChecked = i == index;
            if (current == index)
                return;
            current = index;
            if (notify)
                changed(index);
        }
        for (int i = 0; i < options.Count; i++)
        {
            var index = i;
            var b = new ToggleButton { Content = options[i], Padding = new Thickness(12, 4), CornerRadius = new CornerRadius(7), BorderThickness = new Thickness(0), IsChecked = i == selected };
            if (tips is not null && i < tips.Count)
                ToolTip.SetTip(b, tips[i]);
            b.Click += (_, _) => Select(index, true);
            buttons.Add(b);
            row.Children.Add(b);
        }
        current = selected;
        return (box, i => Select(i, false));
    }

    /// <summary>
    /// Floating panel over the map (rounded, with a soft shadow).
    /// </summary>
    public static Border Floating(Control content)
    {
        var b = new Border
        {
            Child = content,
            CornerRadius = new CornerRadius(10),
            Padding = new Thickness(4),
            BoxShadow = new BoxShadows(new BoxShadow { Blur = 12, OffsetY = 2, Color = Color.FromArgb(0x40, 0, 0, 0) }),
            BorderThickness = new Thickness(1),
        };
        b.Bind(Border.BackgroundProperty, b.GetResourceObservable("NhCardBrush"));
        b.Bind(Border.BorderBrushProperty, b.GetResourceObservable("NhCardBorderBrush"));
        return b;
    }

    /// <summary>
    /// A small "card" header line: bold text and a subtle hint below.
    /// </summary>
    public static StackPanel Heading(string title, string? hint)
    {
        var p = new StackPanel { Spacing = 2 };
        p.Children.Add(Text(title, "cardtitle"));
        if (!string.IsNullOrEmpty(hint))
            p.Children.Add(Text(hint, "subtle"));
        return p;
    }
}
