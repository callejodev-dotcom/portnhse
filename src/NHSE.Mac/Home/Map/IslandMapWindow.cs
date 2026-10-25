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
/// Friendly island map: the island drawn like the game's map, with every tree, flower and piece of furniture.
/// Click a spot to see what is there and change it, turn it or remove it; big items keep all their squares in step.
/// Works on a copy of the map: the save only changes when "Save" is clicked.
/// </summary>
public sealed class IslandMapWindow : NhseWindow
{
    private readonly MainSave Main;
    private readonly MapTileManager Map;
    private readonly LayerFieldItem[] Layers;
    private readonly (int X, int Y) Origin;
    private readonly IslandMapView View;
    private readonly IReadOnlyList<MapBuilding> Buildings;
    private readonly ContentControl Details = new();
    private readonly TextBlock LayerHint = new() { TextWrapping = TextWrapping.Wrap, FontSize = 12, MaxWidth = 300 };
    private List<ComboItem>? ClassicItems;

    private int ActiveLayer;
    private (int X, int Y)? Selected; // tile of the active layer
    private MapBuilding? SelectedBuilding;
    private string? Message;

    /// <summary>
    /// Opens the map as a sheet of the home window; true if it was saved.
    /// </summary>
    public static Task<bool> Open(HomeContext ctx) => ctx.Edit(new IslandMapWindow(ctx.SAV));

    public IslandMapWindow(HorizonSave sav)
    {
        Main = sav.Main;
        Map = MapTileManagerUtil.FromSaveFile(Main);
        Layers = [Map.FieldItems.Layer0, Map.FieldItems.Layer1];
        Origin = Map.ConfigItems.GetCoordinatesAbsolute();
        Buildings = GetBuildings(sav, Map);

        Title = MapStrings.MapTitle;
        Width = 1280;
        Height = 840;
        MinWidth = 960;
        MinHeight = 620;

        var plaza = new Rect(Map.Plaza.X, Map.Plaza.Z, 24, 20);
        View = new IslandMapView(null, Buildings, plaza, Layers, Origin);
        View.TileClicked += ClickTile;
        GroundReady = PaintGround();

        // Header
        var header = new StackPanel { Margin = new Thickness(0, 0, 0, 12) };
        header.Children.Add(Text(MapStrings.MapTitle, "title"));
        header.Children.Add(Text(MapStrings.MapHint, "subtle"));

        // Map with floating controls
        var mapHost = new Grid();
        mapHost.Children.Add(View);
        mapHost.Children.Add(LayerControls());
        mapHost.Children.Add(ZoomControls());
        var mapCard = new Border { Child = mapHost, CornerRadius = new CornerRadius(12), ClipToBounds = true, BorderThickness = new Thickness(1) };
        mapCard.Bind(Border.BorderBrushProperty, mapCard.GetResourceObservable("NhCardBorderBrush"));

        // Side panel
        var detailsCard = new Border
        {
            Child = new ScrollViewer { Content = Details, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled },
            Classes = { "card" },
            Width = 370,
            Margin = new Thickness(16, 0, 0, 0),
        };

        var body = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto") };
        body.Children.Add(mapCard);
        Grid.SetColumn(detailsCard, 1);
        body.Children.Add(detailsCard);

        // Save / Cancel
        var save = new Button { Content = MapStrings.Save, Classes = { "accent" }, IsDefault = true, MinWidth = 100 };
        save.Click += async (_, _) => await SaveAndClose();
        var cancel = new Button { Content = MapStrings.Cancel, IsCancel = true, MinWidth = 100 };
        cancel.Click += (_, _) => Close(false);
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, HorizontalAlignment = HorizontalAlignment.Right };
        buttons.Children.Add(cancel);
        buttons.Children.Add(save);
        var footer = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto"), Margin = new Thickness(0, 14, 0, 0) };
        var hint = Text(MapStrings.NavigationHint, "subtle");
        hint.VerticalAlignment = VerticalAlignment.Center;
        footer.Children.Add(hint);
        Grid.SetColumn(buttons, 1);
        footer.Children.Add(buttons);

        var root = new DockPanel { Margin = new Thickness(22, 18, 22, 18) };
        DockPanel.SetDock(header, Dock.Top);
        DockPanel.SetDock(footer, Dock.Bottom);
        root.Children.Add(header);
        root.Children.Add(footer);
        root.Children.Add(body);
        Content = root;
        this.Bind(BackgroundProperty, this.GetResourceObservable("NhWindowBrush"));

        SetLayer(0);
    }

    /// <summary>
    /// Finishes when the island's ground is painted and shown (it is painted in the background, so the window opens at once).
    /// </summary>
    public Task GroundReady { get; }

    private async Task PaintGround()
    {
        var map = Map;
        var pixels = await Task.Run(() => IslandPainter.PaintPixels(map));
        View.SetGround(ImageUtil.GetBitmap(pixels, IslandPainter.SquaresWide * IslandPainter.PixelsPerSquare, IslandPainter.SquaresHigh * IslandPainter.PixelsPerSquare));
    }

    #region Map controls
    private Control LayerControls()
    {
        var (seg, _) = TilePanel.Segmented([MapStrings.LayerGround, MapStrings.LayerTop], 0, SetLayer, [MapStrings.LayerGroundHint, MapStrings.LayerTopHint]);
        var stack = new StackPanel { Spacing = 4 };
        stack.Children.Add(seg);
        LayerHint.Margin = new Thickness(6, 0, 6, 2);
        LayerHint.Bind(TextBlock.ForegroundProperty, LayerHint.GetResourceObservable("NhSubtleBrush"));
        stack.Children.Add(LayerHint);
        var box = TilePanel.Floating(stack);
        box.HorizontalAlignment = HorizontalAlignment.Left;
        box.VerticalAlignment = VerticalAlignment.Top;
        box.Margin = new Thickness(12);
        return box;
    }

    private Control ZoomControls()
    {
        Button Round(string text, string tip, Action action)
        {
            var b = new Button { Content = text, Width = 34, Height = 32, FontSize = 17, Padding = new Thickness(0), HorizontalContentAlignment = HorizontalAlignment.Center, VerticalContentAlignment = VerticalAlignment.Center };
            ToolTip.SetTip(b, tip);
            b.Click += (_, _) => action();
            return b;
        }
        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 4 };
        row.Children.Add(Round("−", MapStrings.ZoomOut, () => View.ZoomBy(1 / 1.5)));
        row.Children.Add(Round("+", MapStrings.ZoomIn, () => View.ZoomBy(1.5)));
        row.Children.Add(Round("⤢", MapStrings.ZoomFit, View.FitIsland));
        var grid = new ToggleButton { Content = MapStrings.ShowGrid, Height = 32, Padding = new Thickness(10, 0), VerticalContentAlignment = VerticalAlignment.Center };
        ToolTip.SetTip(grid, MapStrings.ShowGridHint);
        grid.IsCheckedChanged += (_, _) => View.ShowGrid = grid.IsChecked == true;
        row.Children.Add(grid);
        var box = TilePanel.Floating(row);
        box.HorizontalAlignment = HorizontalAlignment.Right;
        box.VerticalAlignment = VerticalAlignment.Top;
        box.Margin = new Thickness(12);
        return box;
    }

    private void SetLayer(int layer)
    {
        ActiveLayer = layer;
        View.ActiveLayer = layer;
        LayerHint.Text = layer == 0 ? MapStrings.LayerGroundHint : MapStrings.LayerTopHint;
        Message = null;
        if (Selected is { } s)
            SelectTile(s.X, s.Y);
        else
            BuildDetails();
    }
    #endregion

    #region Selection
    private LayerFieldItem Layer => Layers[ActiveLayer];

    /// <summary>
    /// What a click on the map does: chooses the item, building or empty square at that map position (item tiles of the whole map).
    /// </summary>
    public void ClickTile(int absX, int absY)
    {
        Message = null;
        var x = absX - Origin.X;
        var y = absY - Origin.Y;
        var layer = Layer;
        if (layer.Contains(x, y) && GridItems.TryGetRoot(layer, x, y, out _, out var rx, out var ry))
        {
            SelectTile(rx, ry);
            return;
        }
        if (View.BuildingAt(absX, absY) is { } b)
        {
            Selected = null;
            SelectedBuilding = b;
            View.SelectionFacing = null;
            View.Selection = b.Bounds;
            BuildDetails();
            return;
        }
        if (!layer.Contains(x, y))
        {
            ClearSelection();
            return;
        }
        // An empty spot: a whole square, where new things go.
        SelectTile(x & ~1, y & ~1);
    }

    /// <summary>
    /// Chooses the spot at a tile of the active layer (relative to the layer).
    /// </summary>
    public void SelectTile(int x, int y)
    {
        var layer = Layer;
        if (!layer.Contains(x, y))
            return;
        if (GridItems.TryGetRoot(layer, x, y, out _, out var rx, out var ry))
            (x, y) = (rx, ry);
        Selected = (x, y);
        SelectedBuilding = null;
        UpdateSelectionArea();
        if (View.Selection is { } area)
            View.Reveal(area);
        BuildDetails();
    }

    private void ClearSelection()
    {
        Selected = null;
        SelectedBuilding = null;
        View.Selection = null;
        BuildDetails();
    }

    private void UpdateSelectionArea()
    {
        if (Selected is not { } s)
        {
            View.Selection = null;
            return;
        }
        var item = Layer.GetTile(s.X, s.Y);
        var (w, h) = item.IsNone || !item.IsRoot ? (2, 2) : GridItems.GetSize(item);
        View.SelectionFacing = GridItems.CanRotate(item) ? item.Rotation : null;
        View.Selection = new Rect(s.X + Origin.X, s.Y + Origin.Y, w, h);
    }

    /// <summary>
    /// The item at the chosen spot (its main tile), or an empty item.
    /// </summary>
    public Item SelectedItem => Selected is { } s ? Layer.GetTile(s.X, s.Y) : Item.NO_ITEM;
    #endregion

    #region Changes
    /// <summary>
    /// Puts an item at the chosen spot, replacing what is there. Things that aren't furniture lie on the ground, as when dropped in the game.
    /// </summary>
    public PlaceResult PutItem(Item item)
    {
        if (Selected is not { } s)
            return PlaceResult.OutOfBounds;
        var copy = new Item();
        copy.CopyFrom(item);
        if (ActiveLayer == 0 && !copy.IsFieldItem && !copy.IsNone && !GridItems.CanRotate(copy) && copy.SystemParam == 0)
            copy.IsDropped = true;

        var result = GridItems.Place(Layer, copy, s.X, s.Y, out var blocker, out var at);
        Message = result switch
        {
            PlaceResult.Collision => string.Format(MapStrings.DoesNotFit, blocker is null ? "?" : GridItems.GetName(blocker)),
            PlaceResult.OutOfBounds => MapStrings.OutOfMap,
            _ => null,
        };
        if (result == PlaceResult.Ok)
        {
            Selected = at;
            View.RefreshItems();
        }
        UpdateSelectionArea();
        BuildDetails();
        return result;
    }

    /// <summary>
    /// Removes what is at the chosen spot (all of its squares).
    /// </summary>
    public void RemoveSelected()
    {
        if (Selected is not { } s)
            return;
        Message = null;
        if (GridItems.Remove(Layer, s.X, s.Y))
            View.RefreshItems();
        Selected = (s.X & ~1, s.Y & ~1);
        UpdateSelectionArea();
        BuildDetails();
    }

    public PlaceResult RotateSelected()
    {
        if (Selected is not { } s)
            return PlaceResult.OutOfBounds;
        var result = GridItems.Rotate(Layer, s.X, s.Y, out var blocker);
        Message = result == PlaceResult.Collision ? string.Format(MapStrings.DoesNotFit, blocker is null ? "?" : GridItems.GetName(blocker)) : null;
        if (result == PlaceResult.Ok)
            View.RefreshItems();
        UpdateSelectionArea();
        BuildDetails();
        return result;
    }

    private async Task SaveAndClose()
    {
        if (Map.FieldItems.GetUnsupportedTiles().Count != 0
            && await Dialogs.Prompt(MessageBoxButtons.YesNo, MapStrings.AskUnsupported) != DialogResult.Yes)
            return;
        Map.SetManager(Main);
        Close(true);
    }

    /// <summary>
    /// Writes the map into the save (what "Save" does, without closing).
    /// </summary>
    public void ApplyToSave() => Map.SetManager(Main);
    #endregion

    #region Side panel
    private void BuildDetails()
    {
        if (SelectedBuilding is { } b)
        {
            Details.Content = BuildingDetails(b);
            return;
        }
        if (Selected is not { } s)
        {
            Details.Content = Summary();
            return;
        }

        var item = Layer.GetTile(s.X, s.Y);
        Details.Content = TilePanel.Build(new TilePanelOptions
        {
            Item = item,
            Where = ActiveLayer == 0 ? MapStrings.WhereGround : MapStrings.WhereTop,
            Message = Message,
            AllowPlants = ActiveLayer == 0,
            Position = (s.X + Origin.X, s.Y + Origin.Y),
            ClassicItems = GetClassicItems(),
            Put = i => PutItem(i),
            Remove = RemoveSelected,
            Rotate = GridItems.CanRotate(item) ? () => RotateSelected() : null,
        });
    }

    private List<ComboItem> GetClassicItems()
    {
        if (ClassicItems is not null)
            return ClassicItems;
        var data = GameInfo.Strings.ItemDataSource.ToList();
        var field = FieldItemList.Items.Select(z => z.Value).ToList();
        data.Add(field, GameInfo.Strings.InternalNameTranslation);
        return ClassicItems = data;
    }

    private Control Summary()
    {
        int trees = 0, flowers = 0, furniture = 0, fences = 0, weeds = 0;
        foreach (var t in Layers[0].Tiles)
        {
            if (!t.IsRoot || t.IsNone)
                continue;
            if (FieldItemList.Items.TryGetValue(t.ItemId, out var def))
            {
                var k = def.Kind;
                if (k.IsTree) trees++;
                else if (k.IsFlower) flowers++;
                else if (k.IsFence) fences++;
                else if (k.IsWeed) weeds++;
            }
            else
            {
                furniture++;
            }
        }

        var panel = new StackPanel { Spacing = 10 };
        panel.Children.Add(Icon("PltPalm", 48));
        panel.Children.Add(Text(MapStrings.NothingSelected, "cardtitle"));
        panel.Children.Add(Text(MapStrings.NothingSelectedHint, "subtle"));
        panel.Children.Add(new TextBlock { Text = MapStrings.OnYourIsland, FontWeight = FontWeight.SemiBold, Margin = new Thickness(0, 10, 0, 0) });
        var grid = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto"), ColumnSpacing = 10, RowSpacing = 6 };
        void Row(int r, string icon, string label, int count)
        {
            grid.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
            var img = Icon(icon, 26);
            Grid.SetRow(img, r);
            grid.Children.Add(img);
            var l = new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center };
            Grid.SetRow(l, r);
            Grid.SetColumn(l, 1);
            grid.Children.Add(l);
            var c = new TextBlock { Text = count.ToString("N0"), FontWeight = FontWeight.SemiBold, VerticalAlignment = VerticalAlignment.Center };
            Grid.SetRow(c, r);
            Grid.SetColumn(c, 2);
            grid.Children.Add(c);
        }
        Row(0, "PltOak", MapStrings.CountTrees, trees);
        Row(1, "Turip0", MapStrings.CountFlowers, flowers);
        Row(2, "Lantern", MapStrings.CountFurniture, furniture);
        Row(3, "Fence", MapStrings.CountFences, fences);
        Row(4, "Weed", MapStrings.CountWeeds, weeds);
        panel.Children.Add(grid);
        return panel;
    }

    private static Control BuildingDetails(MapBuilding b)
    {
        var panel = new StackPanel { Spacing = 10 };
        var picture = b.Portrait ?? b.Icon;
        if (picture is not null)
        {
            var round = new Border { Width = 96, Height = 96, CornerRadius = new CornerRadius(48), ClipToBounds = true, HorizontalAlignment = HorizontalAlignment.Left };
            round.Bind(Border.BackgroundProperty, round.GetResourceObservable("NhSelectedBrush"));
            round.Child = new Image { Source = picture, Stretch = Stretch.UniformToFill, Margin = new Thickness(b.Portrait is null ? 16 : 0) };
            panel.Children.Add(round);
        }
        var title = b.IsHouse && b.Name != MapStrings.EmptyLot ? string.Format(MapStrings.HouseOf, b.Name) : b.Name;
        var name = Text(title, "cardtitle");
        name.FontSize = 17;
        panel.Children.Add(name);
        panel.Children.Add(Text(MapStrings.BuildingHere, "subtle"));
        return panel;
    }
    #endregion

    #region Keyboard
    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.Source is TextBox or ComboBox or NumericUpDown or ListBox or ListBoxItem)
        {
            base.OnKeyDown(e);
            return;
        }
        var cmd = UiUtil.CommandModifier;
        switch (e.Key)
        {
            case Key.Delete or Key.Back when Selected is not null:
                RemoveSelected();
                break;
            case Key.Left: Move(-2, 0); break;
            case Key.Right: Move(2, 0); break;
            case Key.Up: Move(0, -2); break;
            case Key.Down: Move(0, 2); break;
            case Key.OemPlus or Key.Add when e.KeyModifiers == cmd:
                View.ZoomBy(1.5);
                break;
            case Key.OemMinus or Key.Subtract when e.KeyModifiers == cmd:
                View.ZoomBy(1 / 1.5);
                break;
            case Key.D0 when e.KeyModifiers == cmd:
                View.FitIsland();
                break;
            default:
                base.OnKeyDown(e);
                return;
        }
        e.Handled = true;
    }

    private void Move(int dx, int dy)
    {
        if (Selected is not { } s)
            return;
        var item = Layer.GetTile(s.X, s.Y);
        var (w, h) = item.IsNone || !item.IsRoot ? (2, 2) : GridItems.GetSize(item);
        var x = dx > 0 ? s.X + w : dx < 0 ? s.X - 1 : s.X;
        var y = dy > 0 ? s.Y + h : dy < 0 ? s.Y - 1 : s.Y;
        if (!Layer.Contains(x, y))
            return;
        Message = null;
        if (GridItems.TryGetRoot(Layer, x, y, out _, out var rx, out var ry))
            SelectTile(rx, ry);
        else
            SelectTile(x & ~1, y & ~1);
    }
    #endregion

    #region Buildings
    private static readonly Color[] RoofPalette =
    [
        Color.FromRgb(0xD5, 0x5A, 0x4C), // red
        Color.FromRgb(0x4D, 0x80, 0xC4), // blue
        Color.FromRgb(0x5E, 0xA4, 0x5A), // green
        Color.FromRgb(0xE3, 0xB0, 0x3E), // yellow
        Color.FromRgb(0x8E, 0x6C, 0xC0), // purple
        Color.FromRgb(0xE0, 0x7E, 0x3C), // orange
        Color.FromRgb(0x70, 0x71, 0x7A), // gray
        Color.FromRgb(0x8B, 0x5E, 0x3C), // brown
    ];

    /// <summary>
    /// A colour for a house's roof: the variants of each roof style are lettered A–H in the game.
    /// </summary>
    private static Color GetRoofColor(RoofType roof)
    {
        var name = roof.ToString();
        var letter = name.Length > 0 ? name[^1] : 'A';
        return letter is >= 'A' and <= 'H' ? RoofPalette[letter - 'A'] : RoofPalette[7];
    }

    /// <summary>
    /// The buildings of the island, with a plain name, a colour, a picture and the size they take on the map.
    /// </summary>
    public static IReadOnlyList<MapBuilding> GetBuildings(HorizonSave sav, MapTileManager map)
    {
        var main = sav.Main;
        var villagers = main.GetVillagers();
        var villagerHouses = main.GetVillagerHouses();
        var playerHouses = main.GetPlayerHouses();
        var result = new List<MapBuilding>();
        var buildings = map.LayerBuildings.Buildings;
        for (int i = 0; i < buildings.Count; i++)
        {
            var b = buildings[i];
            var type = b.BuildingType;
            if (type == BuildingType.None)
                continue;
            string name;
            Color roof;
            IImage? icon = null, portrait = null;
            double w, h;
            switch (type)
            {
                case >= BuildingType.PlayerHouse1 and <= BuildingType.PlayerHouse8:
                {
                    var index = type - BuildingType.PlayerHouse1;
                    name = index < sav.Players.Count ? sav.Players[index].Personal.PlayerName : MapStrings.EmptyLot;
                    roof = index < playerHouses.Length ? GetRoofColor(playerHouses[index].RoofUniqueID) : RoofPalette[0];
                    portrait = index < sav.Players.Count ? GetPhoto(sav.Players[index].Personal) : null;
                    icon = GridItems.HomeIcon("Post");
                    (w, h) = (10, 8);
                    break;
                }
                case >= BuildingType.Villager1 and <= BuildingType.Villager10:
                {
                    var index = type - BuildingType.Villager1;
                    var house = index < villagerHouses.Length ? villagerHouses[index] : null;
                    var v = house is not null && (uint)house.NPC1 < villagers.Length ? villagers[house.NPC1] : null;
                    name = v is null ? MapStrings.EmptyLot : GameInfo.Strings.GetVillager(v.InternalName);
                    roof = house is null ? RoofPalette[7] : GetRoofColor(house.RoofUniqueID);
                    portrait = v is null ? null : VillagerSprite.GetVillagerSprite(v.InternalName);
                    (w, h) = (9, 7);
                    break;
                }
                case BuildingType.ResidentCenterStructure:
                    (name, roof, icon, w, h) = (MapStrings.ResidentServices, Color.FromRgb(0x4E, 0xA2, 0x78), GridItems.HomeIcon("Leaf"), 16, 11);
                    break;
                case BuildingType.ResidentCenterTent:
                    (name, roof, icon, w, h) = (MapStrings.ResidentServices, Color.FromRgb(0x6C, 0xB4, 0x6A), GridItems.HomeIcon("Leaf"), 12, 9);
                    break;
                case BuildingType.NooksCranny:
                    (name, roof, icon, w, h) = (MapStrings.NooksCranny, Color.FromRgb(0x3B, 0x8E, 0x8A), GridItems.HomeIcon("YellowPaperBag"), 14, 9);
                    break;
                case BuildingType.Museum:
                    (name, roof, icon, w, h) = (MapStrings.Museum, Color.FromRgb(0x8C, 0x6E, 0x55), GridItems.HomeIcon("Fossil"), 14, 10);
                    break;
                case BuildingType.AblesSisters:
                    (name, roof, icon, w, h) = (MapStrings.AbleSisters, Color.FromRgb(0xDF, 0x7C, 0x8E), GridItems.HomeIcon("Onepiece"), 10, 8);
                    break;
                case BuildingType.Airport:
                    (name, roof, icon, w, h) = (MapStrings.Airport, Color.FromRgb(0x4D, 0x80, 0xC4), GridItems.HomeIcon("PlaneTicket"), 18, 10);
                    break;
                case BuildingType.Campsite:
                    (name, roof, icon, w, h) = (MapStrings.Campsite, Color.FromRgb(0xE9, 0xC4, 0x6A), null, 10, 8);
                    break;
                case BuildingType.Studio:
                    (name, roof, icon, w, h) = (MapStrings.Studio, Color.FromRgb(0x9C, 0x7B, 0xC9), GridItems.HomeIcon("CameraAppPro"), 12, 8);
                    break;
                case BuildingType.Hotel:
                    (name, roof, icon, w, h) = (MapStrings.Hotel, Color.FromRgb(0x3F, 0xA7, 0xB5), GridItems.HomeIcon("PlaneTicket"), 12, 8);
                    break;
                case BuildingType.ReddsTreasureTrawler:
                    (name, roof, icon, w, h) = (MapStrings.ReddsBoat, Color.FromRgb(0x6B, 0x5B, 0xA0), GridItems.HomeIcon("Haniwa"), 12, 6);
                    break;
                case BuildingType.Bridge:
                    (name, roof, w, h) = (MapStrings.Bridge, Color.FromRgb(0xB9, 0x8A, 0x5A), 10, 5);
                    break;
                case BuildingType.Incline:
                    (name, roof, w, h) = (MapStrings.Incline, Color.FromRgb(0xCF, 0xC4, 0xB0), 8, 4);
                    break;
                default:
                    (name, roof, w, h) = (MapStrings.Building, Color.FromRgb(0xA0, 0x8C, 0x70), 8, 6);
                    break;
            }
            var angle = b.Angle < 4 ? b.Angle * 90d : 45 + ((b.Angle - 4) * 90d);
            result.Add(new MapBuilding(type, b.X, b.Y, w, h, name, roof, icon, portrait, angle, i));
        }
        return result;
    }

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
}
