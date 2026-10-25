using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Friendly editor of the players' houses: choose the house and the room, see the room from above with its furniture
/// on the floor, the things on the tables, rugs, walls and ceiling, and click a spot to change, turn or remove what is there.
/// The house's size, roof, walls and door, the room's wallpaper and flooring are chosen by name; the raw values are in
/// "Advanced options". Works on copies: the save only changes when "Save" is clicked.
/// </summary>
public sealed class HouseWindow : NhseWindow
{
    private readonly MainSave Main;
    private readonly IReadOnlyList<Player> Players;
    private readonly IPlayerHouse[] Houses;
    private readonly int HouseCount;
    private readonly Dictionary<(int House, int Room), RoomManager> Rooms = [];

    private readonly RoomView View = new();
    private readonly ContentControl RoomTabs = new();
    private readonly ContentControl HouseChips = new();
    private readonly ContentControl SpotCard = new();
    private readonly ContentControl RoomCard = new();
    private readonly ContentControl HouseCard = new();
    private readonly TextBlock ModeHint = new() { TextWrapping = TextWrapping.Wrap };
    private readonly Control WallChooser;

    private Action<int>? ShowModeChoice;
    private Action<int>? ShowWallChoice;
    private Action<int>? ShowRoomChoice;
    private IReadOnlyList<int> VisibleRooms = [0];

    private int HouseIndex;
    private int RoomIndex;
    private (int X, int Y)? Selected;
    private string? Message;
    private bool ChoosingWallpaper;
    private bool ChoosingFlooring;

    /// <summary>
    /// Opens the houses as a sheet of the home window, on the selected player's house; true if it was saved.
    /// </summary>
    public static Task<bool> Open(HomeContext ctx) => ctx.Edit(new HouseWindow(ctx.SAV, ctx.PlayerIndex));

    public HouseWindow(HorizonSave sav, int playerIndex = 0)
    {
        Main = sav.Main;
        Players = sav.Players;
        Houses = Main.GetPlayerHouses();
        HouseCount = Math.Max(1, Math.Min(Players.Count, Houses.Length));
        HouseIndex = Math.Clamp(playerIndex, 0, HouseCount - 1);

        Title = MapStrings.HouseTitle;
        Width = 1240;
        Height = 860;
        MinWidth = 980;
        MinHeight = 660;

        View.TileClicked += ClickTile;

        var header = new StackPanel { Margin = new Thickness(0, 0, 0, 10) };
        header.Children.Add(Text(MapStrings.HouseTitle, "title"));
        header.Children.Add(Text(MapStrings.HouseHint, "subtle"));
        HouseChips.Margin = new Thickness(0, 10, 0, 0);
        header.Children.Add(HouseChips);

        // Left: room tabs, what to edit, the room
        var (modes, showMode) = TilePanel.Segmented(
            [MapStrings.EditFloor, MapStrings.EditTables, MapStrings.EditRugs, MapStrings.EditWalls, MapStrings.EditCeiling], 0, i => SetMode((RoomEditMode)i),
            [MapStrings.EditFloorHint, MapStrings.EditTablesHint, MapStrings.EditRugsHint, MapStrings.EditWallsHint, MapStrings.EditCeilingHint]);
        var (walls, showWall) = TilePanel.Segmented([MapStrings.WallBack, MapStrings.WallLeft, MapStrings.WallFront, MapStrings.WallRight], 0,
            i => SetWall((RoomLayerSurface)((int)RoomLayerSurface.WallNorth + i)));
        ShowModeChoice = showMode;
        ShowWallChoice = showWall;
        WallChooser = walls;
        WallChooser.IsVisible = false;
        ModeHint.Bind(TextBlock.ForegroundProperty, ModeHint.GetResourceObservable("NhSubtleBrush"));

        var tools = new StackPanel { Spacing = 8 };
        tools.Children.Add(RoomTabs);
        var modeRow = new WrapPanel { ItemSpacing = 12, LineSpacing = 6 };
        modeRow.Children.Add(modes);
        modeRow.Children.Add(WallChooser);
        tools.Children.Add(modeRow);
        tools.Children.Add(ModeHint);

        var roomCard = new Border { Classes = { "card" }, Padding = new Thickness(8), Child = View };
        var left = new DockPanel();
        DockPanel.SetDock(tools, Dock.Top);
        left.Children.Add(tools);
        roomCard.Margin = new Thickness(0, 10, 0, 0);
        left.Children.Add(roomCard);

        // Right: the chosen spot, the room, the house
        var side = new StackPanel { Spacing = 12 };
        side.Children.Add(new Border { Classes = { "card" }, Child = SpotCard });
        side.Children.Add(new Border { Classes = { "card" }, Child = RoomCard });
        side.Children.Add(new Border { Classes = { "card" }, Child = HouseCard });
        var sideScroll = new ScrollViewer { Content = side, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled, Width = 380, Margin = new Thickness(16, 0, 0, 0) };

        var body = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto") };
        body.Children.Add(left);
        Grid.SetColumn(sideScroll, 1);
        body.Children.Add(sideScroll);

        var save = new Button { Content = MapStrings.Save, Classes = { "accent" }, IsDefault = true, MinWidth = 100 };
        save.Click += (_, _) =>
        {
            ApplyToSave();
            Close(true);
        };
        var cancel = new Button { Content = MapStrings.Cancel, IsCancel = true, MinWidth = 100 };
        cancel.Click += (_, _) => Close(false);
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, HorizontalAlignment = HorizontalAlignment.Right, Margin = new Thickness(0, 14, 0, 0) };
        buttons.Children.Add(cancel);
        buttons.Children.Add(save);

        var root = new DockPanel { Margin = new Thickness(22, 18, 22, 18) };
        DockPanel.SetDock(header, Dock.Top);
        DockPanel.SetDock(buttons, Dock.Bottom);
        root.Children.Add(header);
        root.Children.Add(buttons);
        root.Children.Add(body);
        Content = root;
        this.Bind(BackgroundProperty, this.GetResourceObservable("NhWindowBrush"));

        SetMode(RoomEditMode.Floor);
        ShowHouse(HouseIndex);
    }

    #region Houses and rooms
    private IPlayerHouse House => Houses[HouseIndex];

    private RoomManager GetRoom(int house, int room)
    {
        if (!Rooms.TryGetValue((house, room), out var m))
            Rooms[(house, room)] = m = new RoomManager(Houses[house].GetRoom(room));
        return m;
    }

    private RoomManager Room => GetRoom(HouseIndex, RoomIndex);

    /// <summary>
    /// Rooms the house has: one per expansion (main, back, left, right, upstairs, basement), and any room with things in it.
    /// </summary>
    public IReadOnlyList<int> GetRooms(int house)
    {
        var level = (int)Houses[house].HouseLevel;
        var built = Math.Clamp(level - 2, 1, PlayerHouse1.MaxRoom);
        var result = new List<int>();
        for (int r = 0; r < PlayerHouse1.MaxRoom; r++)
        {
            if (r < built || GetRoom(house, r).Layers.Any(l => l.Tiles.Any(t => !t.IsNone)))
                result.Add(r);
        }
        return result;
    }

    private static string RoomName(int room) => room switch
    {
        0 => MapStrings.RoomMain,
        1 => MapStrings.RoomBack,
        2 => MapStrings.RoomLeft,
        3 => MapStrings.RoomRight,
        4 => MapStrings.RoomUpstairs,
        _ => MapStrings.RoomBasement,
    };

    /// <summary>
    /// Squares across the room: its usual size, or more if things are placed further out.
    /// </summary>
    private int GetRoomSquares(int house, int room)
    {
        var level = Houses[house].HouseLevel;
        int size = room switch
        {
            0 => level >= 3 ? 8 : 6,
            4 => 8,
            5 => 10,
            _ => 6,
        };
        var layers = GetRoom(house, room).Layers;
        for (int li = 0; li < layers.Length; li++)
        {
            bool wall = li is >= (int)RoomLayerSurface.WallNorth and <= (int)RoomLayerSurface.WallEast;
            for (int x = 0; x < RoomView.GridSize; x++)
            {
                for (int y = 0; y < RoomView.GridSize; y++)
                {
                    if (layers[li].GetTile(x, y).IsNone)
                        continue;
                    size = Math.Max(size, Math.Max(10 - x, x - 9));
                    if (!wall)
                        size = Math.Max(size, Math.Max(10 - y, y - 9));
                }
            }
        }
        return Math.Min(size, 10);
    }

    public void ShowHouse(int index)
    {
        HouseIndex = Math.Clamp(index, 0, HouseCount - 1);
        BuildHouseChips();
        BuildRoomTabs(0);
        ShowRoom(VisibleRooms[0]);
        BuildHouseCard();
    }

    private void BuildRoomTabs(int selected)
    {
        var rooms = GetRooms(HouseIndex);
        VisibleRooms = rooms;
        var (tabs, show) = TilePanel.Segmented(rooms.Select(RoomName).ToArray(), selected, i => ShowRoom(rooms[i]));
        RoomTabs.Content = tabs;
        ShowRoomChoice = show;
    }

    public void ShowRoom(int room)
    {
        RoomIndex = room;
        var tab = VisibleRooms.ToList().IndexOf(room);
        if (tab >= 0)
            ShowRoomChoice?.Invoke(tab);
        Selected = null;
        Message = null;
        ChoosingWallpaper = ChoosingFlooring = false;
        var fw = GetFloorWall();
        View.SetRoom(Room.Layers, GetRoomSquares(HouseIndex, room), Picture(fw.FloorItem.UniqueID), Picture(fw.WallItem.UniqueID));
        View.Selection = null;
        BuildSpotCard();
        BuildRoomCard();
    }

    private static IImage? Picture(ushort id) => id is Item.NONE or 0 ? null : ItemSprite.GetItemSprite(id);

    private GSaveRoomFloorWall GetFloorWall() => Room.Room is PlayerRoom1 r ? r.RoomFloorWall : default;

    private void SetFloorWall(GSaveRoomFloorWall value)
    {
        if (Room.Room is PlayerRoom1 r)
            r.RoomFloorWall = value;
    }

    private void BuildHouseChips()
    {
        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        for (int i = 0; i < HouseCount; i++)
        {
            var index = i;
            var content = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
            var photo = new Border { Width = 28, Height = 28, CornerRadius = new CornerRadius(14), ClipToBounds = true };
            photo.Child = new Image { Source = i < Players.Count ? GetPhoto(Players[i].Personal) : null, Stretch = Stretch.UniformToFill };
            content.Children.Add(photo);
            content.Children.Add(new TextBlock { Text = HouseName(i), VerticalAlignment = VerticalAlignment.Center });
            var chip = new ToggleButton { Content = content, IsChecked = i == HouseIndex, Padding = new Thickness(6, 4, 12, 4), CornerRadius = new CornerRadius(18) };
            chip.Click += (_, _) =>
            {
                if (index == HouseIndex)
                    chip.IsChecked = true;
                else
                    ShowHouse(index);
            };
            row.Children.Add(chip);
        }
        HouseChips.Content = row;
    }

    private string HouseName(int index) => string.Format(MapStrings.HouseOf, index < Players.Count ? Players[index].Personal.PlayerName : $"{index + 1}");

    private static Bitmap? GetPhoto(Personal pers)
    {
        try
        {
            return new Bitmap(new MemoryStream(pers.GetPhotoData().ToArray()));
        }
        catch
        {
            return null;
        }
    }
    #endregion

    #region Editing
    public void SetMode(RoomEditMode mode)
    {
        View.Mode = mode;
        ShowModeChoice?.Invoke((int)mode);
        WallChooser.IsVisible = mode == RoomEditMode.Walls;
        ModeHint.Text = mode switch
        {
            RoomEditMode.Tables => MapStrings.EditTablesHint,
            RoomEditMode.Rugs => MapStrings.EditRugsHint,
            RoomEditMode.Walls => MapStrings.EditWallsHint,
            RoomEditMode.Ceiling => MapStrings.EditCeilingHint,
            _ => MapStrings.EditFloorHint,
        };
        Selected = null;
        Message = null;
        View.Selection = null;
        BuildSpotCard();
    }

    public void SetWall(RoomLayerSurface wall)
    {
        View.Wall = wall;
        ShowWallChoice?.Invoke((int)wall - (int)RoomLayerSurface.WallNorth);
        Selected = null;
        View.Selection = null;
        BuildSpotCard();
    }

    private LayerRoomItem Layer => Room.Layers[(int)View.ActiveSurface];

    /// <summary>
    /// Chooses the spot at a tile of the layer being edited (what a click on the room does).
    /// </summary>
    public void ClickTile(int x, int y)
    {
        Message = null;
        ChoosingWallpaper = ChoosingFlooring = false;
        if (GridItems.TryGetRoot(Layer, x, y, out _, out var rx, out var ry))
            Selected = (rx, ry);
        else
            Selected = (x & ~1, y & ~1);
        UpdateSelection();
        BuildSpotCard();
    }

    private void UpdateSelection()
    {
        if (Selected is not { } s)
        {
            View.Selection = null;
            return;
        }
        var item = Layer.GetTile(s.X, s.Y);
        var (w, h) = item.IsNone || !item.IsRoot ? (2, 2) : GridItems.GetSize(item);
        View.SelectionFacing = GridItems.CanRotate(item) && View.Mode != RoomEditMode.Walls ? item.Rotation : null;
        View.Selection = new Rect(s.X, s.Y, w, h);
    }

    public Item SelectedItem => Selected is { } s ? Layer.GetTile(s.X, s.Y) : Item.NO_ITEM;

    /// <summary>
    /// The space of the room where things of the current kind go: the floor, or the wall's face.
    /// </summary>
    private Rect Limits => View.Mode == RoomEditMode.Walls
        ? new Rect(View.Start, 0, View.Span, RoomView.WallRows)
        : new Rect(View.Start, View.Start, View.Span, View.Span);

    private bool Fits(int x, int y, Item item)
    {
        var (w, h) = GridItems.GetSize(item);
        var l = Limits;
        return x >= l.X && y >= l.Y && x + w <= l.Right && y + h <= l.Bottom;
    }

    /// <summary>
    /// Puts an item at the chosen spot, replacing what is there, if it fits in the room.
    /// </summary>
    public PlaceResult PutItem(Item item)
    {
        if (Selected is not { } s)
            return PlaceResult.OutOfBounds;
        var copy = new Item();
        copy.CopyFrom(item);
        var layer = Layer;
        var (x, y) = GridItems.TryGetRoot(layer, s.X, s.Y, out _, out var rx, out var ry) ? (rx, ry) : s;
        PlaceResult result;
        Item? blocker = null;
        if (!Fits(x, y, copy))
        {
            result = PlaceResult.OutOfBounds;
        }
        else
        {
            result = GridItems.Place(layer, copy, x, y, out blocker, out var at, false);
            if (result == PlaceResult.Ok)
                Selected = at;
        }
        AfterChange(result, blocker);
        return result;
    }

    public void RemoveSelected()
    {
        if (Selected is not { } s)
            return;
        GridItems.Remove(Layer, s.X, s.Y);
        AfterChange(PlaceResult.Ok, null);
    }

    public PlaceResult RotateSelected()
    {
        if (Selected is not { } s)
            return PlaceResult.OutOfBounds;
        var item = Layer.GetTile(s.X, s.Y);
        var turned = new Item();
        turned.CopyFrom(item);
        turned.Rotation = (turned.Rotation + 1) & 3;
        Item? blocker = null;
        var result = Fits(s.X, s.Y, turned) ? GridItems.Rotate(Layer, s.X, s.Y, out blocker) : PlaceResult.OutOfBounds;
        AfterChange(result, blocker);
        return result;
    }

    private void AfterChange(PlaceResult result, Item? blocker)
    {
        Message = result switch
        {
            PlaceResult.Collision => string.Format(MapStrings.DoesNotFit, blocker is null ? "?" : GridItems.GetName(blocker)),
            PlaceResult.OutOfBounds => MapStrings.OutOfRoom,
            _ => null,
        };
        UpdateSelection();
        View.Refresh();
        BuildSpotCard();
    }

    /// <summary>
    /// Writes every room and house change into the save (what "Save" does, without closing).
    /// </summary>
    public void ApplyToSave()
    {
        foreach (var ((house, room), m) in Rooms)
        {
            m.Save();
            Houses[house].SetRoom(room, m.Room);
        }
        Main.SetPlayerHouses(Houses);
    }
    #endregion

    #region Side panel
    private string Where => View.Mode switch
    {
        RoomEditMode.Tables => MapStrings.WhereTable,
        RoomEditMode.Rugs => MapStrings.WhereRug,
        RoomEditMode.Walls => $"{MapStrings.WhereWall} · {WallName(View.Wall)}",
        RoomEditMode.Ceiling => MapStrings.WhereCeiling,
        _ => MapStrings.WhereFloor,
    };

    private static string WallName(RoomLayerSurface wall) => wall switch
    {
        RoomLayerSurface.WallWest => MapStrings.WallLeft,
        RoomLayerSurface.WallSouth => MapStrings.WallFront,
        RoomLayerSurface.WallEast => MapStrings.WallRight,
        _ => MapStrings.WallBack,
    };

    private void BuildSpotCard()
    {
        if (ChoosingWallpaper || ChoosingFlooring)
        {
            var p = new StackPanel { Spacing = 10 };
            p.Children.Add(TilePanel.Heading(ChoosingWallpaper ? MapStrings.Wallpaper : MapStrings.Flooring, MapStrings.ChooseWallpaper));
            var search = new ItemSearchBox(300, ItemCategory.Walls);
            search.ItemChosen += (id, _) =>
            {
                var fw = GetFloorWall();
                var chosen = default(GSaveItemName);
                chosen.CopyFrom(new Item(id));
                if (ChoosingWallpaper)
                    fw.WallItem = chosen;
                else
                    fw.FloorItem = chosen;
                SetFloorWall(fw);
                ChoosingWallpaper = ChoosingFlooring = false;
                var now = GetFloorWall();
                View.SetRoom(Room.Layers, GetRoomSquares(HouseIndex, RoomIndex), Picture(now.FloorItem.UniqueID), Picture(now.WallItem.UniqueID));
                BuildSpotCard();
                BuildRoomCard();
            };
            p.Children.Add(search);
            var back = new Button { Content = MapStrings.Cancel };
            back.Click += (_, _) =>
            {
                ChoosingWallpaper = ChoosingFlooring = false;
                BuildSpotCard();
            };
            p.Children.Add(back);
            SpotCard.Content = p;
            return;
        }

        if (Selected is not { } s)
        {
            var p = new StackPanel { Spacing = 8 };
            p.Children.Add(Icon("Lantern", 44));
            p.Children.Add(Text(MapStrings.NothingSelected, "cardtitle"));
            p.Children.Add(Text(MapStrings.RoomPickHint, "subtle"));
            SpotCard.Content = p;
            return;
        }

        var item = Layer.GetTile(s.X, s.Y);
        SpotCard.Content = TilePanel.Build(new TilePanelOptions
        {
            Item = item,
            Where = Where,
            Message = Message,
            Position = s,
            Put = i => PutItem(i),
            Remove = RemoveSelected,
            Rotate = GridItems.CanRotate(item) ? () => RotateSelected() : null,
        });
    }

    private void BuildRoomCard()
    {
        var fw = GetFloorWall();
        var p = new StackPanel { Spacing = 10 };
        p.Children.Add(Text(MapStrings.RoomSection + " · " + RoomName(RoomIndex), "cardtitle"));
        Control Row(string label, GSaveItemName value, Action change)
        {
            var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto"), ColumnSpacing = 10 };
            var img = new Border { Width = 40, Height = 40, CornerRadius = new CornerRadius(8), Child = new Image { Source = Picture(value.UniqueID), Margin = new Thickness(3) } };
            img.Bind(Border.BackgroundProperty, img.GetResourceObservable("NhSelectedBrush"));
            g.Children.Add(img);
            var text = new StackPanel { VerticalAlignment = VerticalAlignment.Center };
            text.Children.Add(Text(label, "subtle"));
            text.Children.Add(new TextBlock { Text = value.UniqueID is Item.NONE or 0 ? "—" : GameInfo.Strings.GetItemName(value.ToItem()), TextTrimming = TextTrimming.CharacterEllipsis });
            Grid.SetColumn(text, 1);
            g.Children.Add(text);
            var button = new Button { Content = MapStrings.Change, VerticalAlignment = VerticalAlignment.Center };
            button.Click += (_, _) => change();
            Grid.SetColumn(button, 2);
            g.Children.Add(button);
            return g;
        }
        p.Children.Add(Row(MapStrings.Wallpaper, fw.WallItem, () =>
        {
            ChoosingWallpaper = true;
            ChoosingFlooring = false;
            BuildSpotCard();
        }));
        p.Children.Add(Row(MapStrings.Flooring, fw.FloorItem, () =>
        {
            ChoosingFlooring = true;
            ChoosingWallpaper = false;
            BuildSpotCard();
        }));
        RoomCard.Content = p;
    }

    private void BuildHouseCard()
    {
        var house = House;
        var p = new StackPanel { Spacing = 8 };
        p.Children.Add(Text(MapStrings.HouseSection, "cardtitle"));

        string[] levels = [MapStrings.LevelTent, MapStrings.LevelHouse, MapStrings.LevelBigger, MapStrings.LevelBack, MapStrings.LevelLeft, MapStrings.LevelRight, MapStrings.LevelUpstairs, MapStrings.LevelBasement];
        var levelNames = levels.ToList();
        var level = (int)house.HouseLevel;
        if (level < 1 || level > levels.Length)
            levelNames.Add(level.ToString());
        var size = new ComboBox { ItemsSource = levelNames, SelectedIndex = level >= 1 && level <= levels.Length ? level - 1 : levels.Length, HorizontalAlignment = HorizontalAlignment.Stretch };
        size.SelectionChanged += (_, _) =>
        {
            if (size.SelectedIndex is < 0 || size.SelectedIndex >= levels.Length)
                return;
            house.HouseLevel = (uint)(size.SelectedIndex + 1);
            var room = RoomIndex;
            BuildRoomTabs(Math.Max(0, GetRooms(HouseIndex).ToList().IndexOf(room)));
            if (!VisibleRooms.Contains(room))
                ShowRoom(VisibleRooms[0]);
        };
        p.Children.Add(Labeled(MapStrings.HouseSize, size));
        p.Children.Add(Labeled(MapStrings.Roof, StylePicker(house.RoofUniqueID, v => house.RoofUniqueID = v)));
        p.Children.Add(Labeled(MapStrings.Walls, StylePicker(house.WallUniqueID, v => house.WallUniqueID = v)));
        p.Children.Add(Labeled(MapStrings.Door, StylePicker(house.DoorUniqueID, v => house.DoorUniqueID = v)));

        // Raw values and files, for those who know
        var advanced = new StackPanel { Spacing = 8 };
        var expander = new Expander { Header = MapStrings.AdvancedOptions, Content = advanced, HorizontalAlignment = HorizontalAlignment.Stretch, Margin = new Thickness(0, 6, 0, 0) };
        expander.PropertyChanged += (_, e) =>
        {
            if (e.Property != Expander.IsExpandedProperty || !expander.IsExpanded || advanced.Children.Count != 0)
                return;
            advanced.Children.Add(new PropertyGrid { SelectedObject = house, PropertySort = PropertySort.NoSort, HelpVisible = false, Height = 300 });
            advanced.Children.Add(Row(SmallButton(MapStrings.EditFlags, EditFlags)));
            advanced.Children.Add(Row(SmallButton(MapStrings.ExportHouse, ExportHouse), SmallButton(MapStrings.ImportHouse, ImportHouse)));
            advanced.Children.Add(Row(SmallButton(MapStrings.ExportRoom, ExportRoom), SmallButton(MapStrings.ImportRoom, ImportRoom)));
        };
        p.Children.Add(expander);
        HouseCard.Content = p;
    }

    private static Grid Labeled(string label, Control control)
    {
        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("80,*"), ColumnSpacing = 8 };
        g.Children.Add(new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center });
        Grid.SetColumn(control, 1);
        g.Children.Add(control);
        return g;
    }

    private static ComboBox StylePicker<T>(T current, Action<T> set) where T : struct, Enum
    {
        var values = Enum.GetValues<T>().Where(v => HouseStyleNames.IsForPlayers(v) || EqualityComparer<T>.Default.Equals(v, current)).ToList();
        if (!values.Contains(current))
            values.Add(current);
        var names = HouseStyleNames.GetNames(values);
        var cb = new ComboBox { ItemsSource = names, SelectedIndex = values.IndexOf(current), HorizontalAlignment = HorizontalAlignment.Stretch, MaxDropDownHeight = 400 };
        cb.SelectionChanged += (_, _) =>
        {
            if (cb.SelectedIndex >= 0)
                set(values[cb.SelectedIndex]);
        };
        return cb;
    }

    private async Task EditFlags()
    {
        var edit = House.EventFlags.ToArray();
        var editor = new PlayerHouseFlagEditor(edit);
        if (await editor.ShowDialog<bool>(this))
            edit.CopyTo(House.EventFlags);
    }

    private Task ExportHouse()
    {
        SaveRooms(HouseIndex);
        return MiscDumpHelper.DumpHouse(this, Players, Houses, HouseIndex, false);
    }

    private async Task ImportHouse()
    {
        if (!await MiscDumpHelper.LoadHouse(this, Main.Offsets, Players, Houses, HouseIndex))
            return;
        foreach (var key in Rooms.Keys.Where(k => k.House == HouseIndex).ToList())
            Rooms.Remove(key);
        ShowHouse(HouseIndex);
    }

    private Task ExportRoom()
    {
        Room.Save();
        return MiscDumpHelper.DumpRoom(this, Room.Room, RoomIndex);
    }

    private async Task ImportRoom()
    {
        Room.Save();
        var room = await MiscDumpHelper.LoadRoom(this, Main.Offsets, Room.Room, RoomIndex);
        if (room is null)
            return;
        House.SetRoom(RoomIndex, room);
        Rooms.Remove((HouseIndex, RoomIndex));
        ShowRoom(RoomIndex);
    }

    private void SaveRooms(int house)
    {
        foreach (var ((h, r), m) in Rooms)
        {
            if (h != house)
                continue;
            m.Save();
            Houses[h].SetRoom(r, m.Room);
        }
    }
    #endregion

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.Source is TextBox or ComboBox or NumericUpDown or ListBox or ListBoxItem)
        {
            base.OnKeyDown(e);
            return;
        }
        if (e.Key is Key.Delete or Key.Back && Selected is not null)
        {
            RemoveSelected();
            e.Handled = true;
            return;
        }
        base.OnKeyDown(e);
    }
}

/// <summary>
/// Plain names for the looks of a house (roof, walls, door): the material and its colour number,
/// from the game's internal names ("HouseRoofNASlateD" → "Slate · color 4").
/// </summary>
public static class HouseStyleNames
{
    private static readonly string[] Prefixes = ["HouseRoof", "HouseWall", "HouseDoor"];
    private static readonly string[] Regions = ["PA00", "PA01", "PA02", "PA03", "PA04", "PB04", "NA", "NB", "NC", "ND"];

    private readonly record struct Parts(string Material, int Color, bool Round, string Family);

    /// <summary>
    /// Looks that a player's house can have (villagers' houses have their own styles).
    /// </summary>
    public static bool IsForPlayers<T>(T value) where T : struct, Enum
    {
        var name = value.ToString();
        if (name.StartsWith('_'))
            return false;
        if (typeof(T) == typeof(DoorKind))
            return !name.StartsWith("HouseTent", StringComparison.Ordinal);
        return name.StartsWith("HouseRoofP", StringComparison.Ordinal) || name.StartsWith("HouseWallP", StringComparison.Ordinal);
    }

    public static string GetName<T>(T value) where T : struct, Enum => Format(Parse(value), 0);

    /// <summary>
    /// Names of several looks. When a material comes in several styles of house, each gets a number ("Classic 2 · color 3").
    /// </summary>
    public static List<string> GetNames<T>(IReadOnlyList<T> values) where T : struct, Enum
    {
        var parts = values.Select(Parse).ToList();
        var names = new List<string>(parts.Count);
        foreach (var p in parts)
        {
            var families = parts.Where(z => z.Material == p.Material).Select(z => z.Family).Distinct().ToList();
            names.Add(Format(p, families.Count > 1 ? families.IndexOf(p.Family) + 1 : 0));
        }
        var seen = new Dictionary<string, int>();
        for (int i = 0; i < names.Count; i++)
        {
            var n = names[i];
            seen[n] = seen.GetValueOrDefault(n) + 1;
            if (seen[n] > 1)
                names[i] = $"{n} ({seen[n]})";
        }
        return names;
    }

    private static string Format(Parts p, int model)
    {
        var text = model > 0 ? $"{p.Material} {model}" : p.Material;
        if (p.Color > 0)
            text += " · " + string.Format(MapStrings.ColorNumber, p.Color);
        return p.Round ? $"{text} ({MapStrings.DoorRound})" : text;
    }

    private static Parts Parse<T>(T value) where T : struct, Enum
    {
        var name = value.ToString();
        if (name.StartsWith('_') || char.IsDigit(name[0]))
            return new($"{MapStrings.StyleOther} {name.TrimStart('_')}", 0, false, string.Empty);
        if (name.StartsWith("HouseTent", StringComparison.Ordinal))
            return new(MapStrings.StyleTent, 0, false, name);
        if (name.Contains("ForSale", StringComparison.Ordinal))
            return new(MapStrings.StyleForSale, 0, false, string.Empty);
        if (name.Contains("SoldOut", StringComparison.Ordinal))
            return new(MapStrings.StyleSoldOut, 0, false, string.Empty);
        if (name.Contains("HouseMoving", StringComparison.Ordinal))
            return new(MapStrings.StyleMoving, 0, false, name);

        foreach (var p in Prefixes)
        {
            if (name.StartsWith(p, StringComparison.Ordinal))
                name = name[p.Length..];
        }
        var family = string.Empty;
        foreach (var r in Regions)
        {
            if (name.StartsWith(r, StringComparison.Ordinal))
            {
                family = r;
                name = name[r.Length..];
                break;
            }
        }

        // Doors end in S (square) or R (round); then a colour letter.
        bool round = false;
        if (typeof(T) == typeof(DoorKind) && name.Length > 2 && name[^1] is 'S' or 'R' && char.IsUpper(name[^2]))
        {
            round = name[^1] == 'R';
            name = name[..^1];
        }
        int color = 0;
        if (name.Length > 1 && name[^1] is >= 'A' and <= 'H' && char.IsLower(name[^2]))
        {
            color = name[^1] - 'A' + 1;
            name = name[..^1];
        }

        var material = name switch
        {
            "Standard" => MapStrings.StyleStandard,
            "Thatched" => MapStrings.StyleThatched,
            "Stone" => MapStrings.StyleStone,
            "Westerntile" => MapStrings.StyleWesternTile,
            "Slate" => MapStrings.StyleSlate,
            "Metal" => MapStrings.StyleMetal,
            "Tile" => MapStrings.StyleTile,
            "Woodtile" => MapStrings.StyleWoodTile,
            "Woodpanel" => MapStrings.StyleWoodPanel,
            "Japanese" => MapStrings.StyleJapanese,
            "Stoneweight" => MapStrings.StyleStoneWeight,
            "Wood" => MapStrings.StyleWood,
            "Woodframe" => MapStrings.StyleWoodFrame,
            "Woodsiding" or "Woodsidng" => MapStrings.StyleWoodSiding,
            "Brick" => MapStrings.StyleBrick,
            "Stucco" => MapStrings.StyleStucco,
            "Log" => MapStrings.StyleLog,
            "Oriental" => MapStrings.StyleOriental,
            "Soil" => MapStrings.StyleSoil,
            "Iron" => MapStrings.StyleIron,
            "Ironparts" => MapStrings.StyleIronParts,
            "Relief" => MapStrings.StyleRelief,
            "Window" => MapStrings.StyleWindow,
            "IronGrill" => MapStrings.StyleIronGrill,
            "Chinese" => MapStrings.StyleChinese,
            "Simple" => MapStrings.StyleSimple,
            "Carving" => MapStrings.StyleCarving,
            "CercleWindow" => MapStrings.StyleRoundWindow,
            "VerticalWindow" => MapStrings.StyleTallWindow,
            "Lattice" => MapStrings.StyleLattice,
            "Simplicity" => MapStrings.StylePlain,
            _ => name,
        };
        return new(material, color, round, family);
    }
}
