using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Friendly editor of a villager's wallpaper and flooring: both as pictures with their names. Click the wall or the
/// floor and choose another one in the list beside it, which shows only wallpapers or only floorings (the "Walls &amp;
/// floors" category of <see cref="ItemSearchBox"/> also has the rugs, which can't go there). The accent wall is shown
/// when the house has one; the raw values (custom design numbers, directions) are under "Advanced options".
/// Read <see cref="Room"/> back when the window returns true.
/// </summary>
public sealed class RoomWindow : NhseWindow
{
    /// <summary>
    /// Plain white wall: what the game keeps as the accent wall of a house that doesn't use one.
    /// </summary>
    private const ushort NoAccentWall = 0x134B;

    private enum Part { Wall, Floor, Accent }

    public GSaveRoomFloorWall Room { get; private set; }

    private readonly StackPanel Parts = new() { Spacing = 10 };
    private readonly TextBlock ChangeTitle = new() { FontWeight = FontWeight.SemiBold, FontSize = 15 };
    private readonly TextBlock ChangeHint;
    private readonly TextBox Search;
    private readonly ListBox Results;
    private readonly CheckBox ShowAccent;
    private PropertyGrid? Raw;
    private Part Selected = Part.Wall;

    public RoomWindow(GSaveRoomFloorWall room, string villagerName)
    {
        Room = room;
        Title = string.Format(VillagerStrings.RoomTitle, villagerName);
        SizeToContent = SizeToContent.WidthAndHeight;
        CanResize = false;

        var accent = room.AccentWallItem.UniqueID;
        ShowAccent = new CheckBox { Content = VillagerStrings.RoomShowAccent, IsChecked = accent is not (NoAccentWall or Item.NONE) };
        ShowAccent.IsCheckedChanged += (_, _) =>
        {
            if (ShowAccent.IsChecked != true && Selected == Part.Accent)
                Selected = Part.Wall;
            Refresh();
        };

        var header = new StackPanel { Margin = new Thickness(0, 0, 0, 14) };
        header.Children.Add(Text(Title, "title"));
        header.Children.Add(Text(VillagerStrings.RoomHint, "subtle"));

        // Right: the wallpapers or the floorings to choose from
        ChangeHint = Text(string.Empty, "subtle");
        Search = new TextBox { PlaceholderText = VillagerStrings.RoomSearch };
        Results = new ListBox
        {
            Height = 330,
            ItemTemplate = new FuncDataTemplate<Entry>((e, _) =>
            {
                var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
                if (e is null)
                    return row;
                row.Children.Add(new Image { Source = ItemSprite.GetItemSprite(e.Id), Width = 28, Height = 28 });
                row.Children.Add(new TextBlock { Text = e.Name, VerticalAlignment = VerticalAlignment.Center, TextTrimming = TextTrimming.CharacterEllipsis });
                return row;
            }),
        };
        Search.TextChanged += (_, _) => Filter();
        Search.KeyDown += (_, e) =>
        {
            if (e.Key == Key.Down && Results.ItemCount > 0)
            {
                Results.SelectedIndex = 0;
                Results.Focus();
                e.Handled = true;
            }
            else if (e.Key == Key.Enter && Results.ItemCount > 0 && Results.Items[0] is Entry first)
            {
                Put(first.Id);
                e.Handled = true;
            }
        };
        Results.Tapped += (_, _) =>
        {
            if (Results.SelectedItem is Entry e)
                Put(e.Id);
        };
        Results.KeyDown += (_, e) =>
        {
            if (e.Key == Key.Enter && Results.SelectedItem is Entry entry)
            {
                Put(entry.Id);
                e.Handled = true;
            }
        };
        var picker = new StackPanel { Spacing = 6 };
        picker.Children.Add(ChangeTitle);
        picker.Children.Add(ChangeHint);
        picker.Children.Add(Search);
        picker.Children.Add(Results);
        var pickerCard = new Border { Child = picker, Classes = { "card" }, Width = 360, Margin = new Thickness(18, 0, 0, 0), VerticalAlignment = VerticalAlignment.Top };

        // Left: what the room has now, and the raw values
        var left = new StackPanel { Width = 330 };
        left.Children.Add(Parts);
        left.Children.Add(Advanced());

        var save = new Button { Content = HomeStrings.Done, Classes = { "accent" }, IsDefault = true, MinWidth = 100 };
        save.Click += (_, _) => Close(true);
        var cancel = new Button { Content = HomeStrings.Cancel, IsCancel = true, MinWidth = 100 };
        cancel.Click += (_, _) => Close(false);
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, HorizontalAlignment = HorizontalAlignment.Right, Margin = new Thickness(0, 16, 0, 0) };
        buttons.Children.Add(cancel);
        buttons.Children.Add(save);

        var body = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,Auto") };
        body.Children.Add(left);
        Grid.SetColumn(pickerCard, 1);
        body.Children.Add(pickerCard);

        var root = new DockPanel { Margin = new Thickness(22, 18, 22, 18) };
        DockPanel.SetDock(header, Dock.Top);
        DockPanel.SetDock(buttons, Dock.Bottom);
        root.Children.Add(header);
        root.Children.Add(buttons);
        root.Children.Add(body);
        Content = root;
        this.Bind(BackgroundProperty, this.GetResourceObservable("NhWindowBrush"));

        Refresh();
    }

    #region Wall, floor and accent wall
    private static string Capitalize(string name) => name.Length == 0 ? name : char.ToUpper(name[0]) + name[1..];

    private static string GetName(GSaveItemName item)
        => item.UniqueID == Item.NONE ? HomeStrings.EmptySlot : Capitalize(GameInfo.Strings.GetItemName(item.ToItem()));

    private GSaveItemName Current => Selected switch
    {
        Part.Floor => Room.FloorItem,
        Part.Accent => Room.AccentWallItem,
        _ => Room.WallItem,
    };

    /// <summary>
    /// Shows the room as it is now and, beside it, the list for the part that is chosen.
    /// </summary>
    private void Refresh()
    {
        Parts.Children.Clear();
        Parts.Children.Add(PartCard(Part.Wall, VillagerStrings.RoomWall, null, Room.WallItem));
        Parts.Children.Add(PartCard(Part.Floor, VillagerStrings.RoomFloor, null, Room.FloorItem));
        if (ShowAccent.IsChecked == true)
            Parts.Children.Add(PartCard(Part.Accent, VillagerStrings.RoomAccent, VillagerStrings.RoomAccentHint, Room.AccentWallItem));

        ChangeTitle.Text = Selected switch
        {
            Part.Floor => VillagerStrings.RoomChangeFloor,
            Part.Accent => VillagerStrings.RoomChangeAccent,
            _ => VillagerStrings.RoomChangeWall,
        };
        ChangeHint.Text = Selected == Part.Floor ? VillagerStrings.RoomPickFloorHint : VillagerStrings.RoomPickWallHint;
        Filter();
    }

    private Button PartCard(Part part, string label, string? hint, GSaveItemName item)
    {
        var picture = new Image { Source = ItemSprite.GetItemSprite(item.UniqueID), Width = 64, Height = 64, VerticalAlignment = VerticalAlignment.Center };
        var text = new StackPanel { Spacing = 2, VerticalAlignment = VerticalAlignment.Center };
        text.Children.Add(Text(label, "subtle"));
        var name = Text(GetName(item), "cardtitle");
        name.FontSize = 17;
        text.Children.Add(name);
        if (hint is not null)
            text.Children.Add(Text(hint, "subtle"));

        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 14 };
        g.Children.Add(picture);
        Grid.SetColumn(text, 1);
        g.Children.Add(text);

        var b = new Button { Content = g, Classes = { "card" } };
        if (part == Selected)
        {
            b.Bind(BorderBrushProperty, b.GetResourceObservable("NhGreenBrush"));
            b.BorderThickness = new Thickness(2);
        }
        b.Click += (_, _) =>
        {
            if (Selected == part)
                return;
            Selected = part;
            Search.Text = string.Empty;
            Refresh();
        };
        return b;
    }

    /// <summary>
    /// Puts a wallpaper on the chosen wall (the walls, or the accent wall) or a flooring on the floor.
    /// Returns false (and changes nothing) for anything else, like a rug.
    /// </summary>
    public bool Put(ushort id)
    {
        var kind = ItemInfo.GetItemKind(id);
        if (id == Current.UniqueID && kind == (Selected == Part.Floor ? ItemKind.Kind_RoomFloor : ItemKind.Kind_RoomWall))
            return true; // already there (a click on the one the list starts at)

        var room = Room;
        if (kind == ItemKind.Kind_RoomWall)
        {
            var name = Selected == Part.Accent ? room.AccentWallItem : room.WallItem;
            name.CopyFrom(new Item(id));
            if (Selected == Part.Accent)
            {
                room.AccentWallItem = name;
            }
            else
            {
                room.WallItem = name;
                Selected = Part.Wall;
            }
        }
        else if (kind == ItemKind.Kind_RoomFloor)
        {
            var name = room.FloorItem;
            name.CopyFrom(new Item(id));
            room.FloorItem = name;
            Selected = Part.Floor;
        }
        else
        {
            return false;
        }

        Room = room;
        Refresh();
        if (Raw is not null)
            Raw.SelectedObject = Room;
        return true;
    }
    #endregion

    #region List of wallpapers and floorings
    private sealed record Entry(ushort Id, string Name, string Key, ItemKind Kind);

    private static string? CachedLanguage;
    private static Entry[] CachedEntries = [];

    private static readonly char[] NotLetter = [.. "¡!¿?()[]\"'«»-—–.,:;#*+ "];

    /// <summary>
    /// Every wallpaper and flooring of the current language, sorted by name.
    /// </summary>
    private static Entry[] GetEntries()
    {
        if (CachedLanguage == GameInfo.CurrentLanguage)
            return CachedEntries;
        CachedLanguage = GameInfo.CurrentLanguage;
        return CachedEntries = [.. GameInfo.Strings.ItemDataSource
            .Select(z => (Id: (ushort)z.Value, Name: z.Text, Kind: ItemInfo.GetItemKind((ushort)z.Value)))
            .Where(z => z.Kind is ItemKind.Kind_RoomWall or ItemKind.Kind_RoomFloor && z.Name.Any(char.IsLetter) && !z.Name.StartsWith("(Item #", StringComparison.Ordinal))
            .Select(z => new Entry(z.Id, Capitalize(z.Name), ItemSearchBox.Normalize(z.Name), z.Kind))
            .OrderBy(z => z.Key.TrimStart(NotLetter), StringComparer.Ordinal)];
    }

    /// <summary>
    /// Wallpapers for the walls, floorings for the floor; matching ignores case and accents and finds every typed word.
    /// </summary>
    private void Filter()
    {
        var kind = Selected == Part.Floor ? ItemKind.Kind_RoomFloor : ItemKind.Kind_RoomWall;
        var words = ItemSearchBox.Normalize(Search.Text ?? string.Empty).Split(' ', StringSplitOptions.RemoveEmptyEntries);
        var matches = GetEntries().Where(z => z.Kind == kind);
        if (words.Length != 0)
        {
            matches = matches.Where(z => words.All(w => z.Key.Contains(w, StringComparison.Ordinal)))
                .OrderBy(z => z.Key.StartsWith(words[0], StringComparison.Ordinal) ? 0 : 1);
        }
        var list = matches.ToArray();
        Results.ItemsSource = list;

        // Without a search, the list starts at the one the house has now.
        var current = words.Length == 0 ? Array.Find(list, z => z.Id == Current.UniqueID) : null;
        Results.SelectedItem = current;
        if (current is not null)
            Results.ScrollIntoView(current);
    }
    #endregion

    #region Advanced
    private Expander Advanced()
    {
        var panel = new StackPanel { Spacing = 6 };
        panel.Children.Add(ShowAccent);
        panel.Children.Add(Text(VillagerStrings.RoomAdvancedHint, "subtle"));
        var expander = new Expander { Header = HomeStrings.AdvancedOptions, Content = panel, HorizontalAlignment = HorizontalAlignment.Stretch, Margin = new Thickness(0, 12, 0, 0) };
        expander.PropertyChanged += (_, e) =>
        {
            if (e.Property != Expander.IsExpandedProperty || !expander.IsExpanded || Raw is not null)
                return;
            Raw = new PropertyGrid { Height = 260, PropertySort = PropertySort.NoSort, SelectedObject = Room };
            Raw.PropertyValueChanged += (_, _) =>
            {
                if (Raw.SelectedObject is GSaveRoomFloorWall edited)
                    Room = edited;
                Refresh();
            };
            panel.Children.Add(Raw);
        };
        return expander;
    }
    #endregion
}
