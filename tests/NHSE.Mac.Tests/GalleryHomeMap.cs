using Avalonia;
using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Input;
using Avalonia.Headless;
using Avalonia.Threading;
using Avalonia.VisualTree;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The friendly island map and house windows (5x-…): screenshots at a few zoom levels, and their editing through a copy of the real save.
/// </summary>
public class GalleryHomeMap
{
    private static HorizonSave Save()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        return TestSave.LoadCopy(out _);
    }

    private static IslandMapWindow ShowMap(HorizonSave sav, out IslandMapView view)
    {
        var w = new IslandMapWindow(sav) { Width = 1280, Height = 840 };
        w.Show();
        var until = DateTime.UtcNow.AddSeconds(30);
        while (!w.GroundReady.IsCompleted && DateTime.UtcNow < until)
        {
            Dispatcher.UIThread.RunJobs();
            Thread.Sleep(10);
        }
        Assert.True(w.GroundReady.IsCompletedSuccessfully);
        Dispatcher.UIThread.RunJobs();
        view = w.GetVisualDescendants().OfType<IslandMapView>().Single();
        return w;
    }

    /// <summary>
    /// A square of grass on the ground layer with nothing on it, away from buildings (relative to the item layer).
    /// </summary>
    private static (int X, int Y) FindEmptySquare(HorizonSave sav, int size = 4)
    {
        var map = MapTileManagerUtil.FromSaveFile(sav.Main);
        var layer = map.FieldItems.Layer0;
        var (ox, oy) = map.ConfigItems.GetCoordinatesAbsolute();
        var buildings = map.LayerBuildings.Buildings.Where(b => b.BuildingType != BuildingType.None).ToArray();
        for (int y = 40; y < layer.TileInfo.TotalHeight - 40; y += 2)
        {
            for (int x = 60; x < layer.TileInfo.TotalWidth - 60; x += 2)
            {
                bool free = true;
                for (int i = 0; i < size && free; i++)
                {
                    for (int j = 0; j < size && free; j++)
                        free = layer.GetTile(x + i, y + j).IsNone;
                }
                if (!free || buildings.Any(b => Math.Abs(b.X - (x + ox)) < 16 && Math.Abs(b.Y - (y + oy)) < 16))
                    continue;
                return (x, y);
            }
        }
        throw new InvalidOperationException("No empty square");
    }

    [AvaloniaFact]
    public void MapWholeIsland()
    {
        var w = ShowMap(Save(), out _);
        Gallery.Capture(w, "50-map-island");
    }

    [AvaloniaFact]
    public void MapZoomed()
    {
        var sav = Save();
        var w = ShowMap(sav, out var view);
        // Around the player's house, close enough to see each flower.
        var house = sav.Main.Buildings.First(b => b.BuildingType == BuildingType.PlayerHouse1);
        view.ShowArea(new Rect(house.X - 34, house.Y - 22, 68, 44));
        Dispatcher.UIThread.RunJobs();
        var p = view.ToScreen(new Point(house.X + 14, house.Y + 6));
        view.SetHover(p);
        Gallery.Capture(w, "51-map-zoom", false);

        // A tree chosen: the panel shows it, with the search to change it.
        var map = MapTileManagerUtil.FromSaveFile(sav.Main);
        var (ox, oy) = map.ConfigItems.GetCoordinatesAbsolute();
        var layer = map.FieldItems.Layer0;
        var tree = Enumerable.Range(0, layer.Tiles.Length).Select(i => (I: i, T: layer.Tiles[i]))
            .First(z => z.T.IsRoot && FieldItemList.Items.TryGetValue(z.T.ItemId, out var d) && d.Kind.IsTree);
        var (tx, ty) = (tree.I / layer.TileInfo.TotalHeight, tree.I % layer.TileInfo.TotalHeight);
        w.SelectTile(tx, ty);
        view.ShowArea(new Rect(tx + ox - 30, ty + oy - 18, 60, 36));
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "52-map-tree", false);

        // Closer, with the grid, a piece of furniture chosen.
        var ftr = Enumerable.Range(0, layer.Tiles.Length).Select(i => (I: i, T: layer.Tiles[i]))
            .First(z => z.T.IsRoot && !z.T.IsNone && !z.T.IsFieldItem && GridItems.CanRotate(z.T));
        var (fx, fy) = (ftr.I / layer.TileInfo.TotalHeight, ftr.I % layer.TileInfo.TotalHeight);
        w.SelectTile(fx, fy);
        view.ShowGrid = true;
        view.ShowArea(new Rect(fx + ox - 14, fy + oy - 9, 28, 18));
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "53-map-furniture");
    }

    [AvaloniaFact]
    public void MapBuildingAndEmptySpot()
    {
        var sav = Save();
        var w = ShowMap(sav, out var view);
        var museum = sav.Main.Buildings.First(b => b.BuildingType == BuildingType.Museum);
        view.ShowArea(new Rect(museum.X - 40, museum.Y - 25, 80, 50));
        Dispatcher.UIThread.RunJobs();
        w.ClickTile(museum.X, museum.Y);
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "54-map-building", false);

        var (x, y) = FindEmptySquare(sav);
        w.SelectTile(x, y);
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "54-map-empty");
    }

    /// <summary>
    /// An item put on an empty square goes to the save (with the extra squares of a big item) only when saving; removing it empties them.
    /// </summary>
    [AvaloniaFact]
    public void MapPlaceAndRemove()
    {
        var sav = Save();
        var (x, y) = FindEmptySquare(sav);
        var before = MapTileManagerUtil.FromSaveFile(sav.Main).FieldItems.Layer0.GetTile(x, y);
        Assert.True(before.IsNone);

        // A 2x2-square piece of furniture (4x4 tiles).
        var big = GameInfo.Strings.ItemDataSource.Select(z => (ushort)z.Value)
            .First(id => id is > 0 and < 60_000 && ItemInfo.GetItemSize(id) == ItemSizeType.S_2_0x2_0 && ItemInfo.GetItemKind(id).IsFurniture);

        var w = new IslandMapWindow(sav);
        w.SelectTile(x, y);
        Assert.Equal(PlaceResult.Ok, w.PutItem(new Item(big)));
        Assert.Equal(big, w.SelectedItem.ItemId);

        // Not in the save yet.
        Assert.True(MapTileManagerUtil.FromSaveFile(sav.Main).FieldItems.Layer0.GetTile(x, y).IsNone);
        w.ApplyToSave();
        var layer = MapTileManagerUtil.FromSaveFile(sav.Main).FieldItems.Layer0;
        Assert.Equal(big, layer.GetTile(x, y).ItemId);
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                if (i == 0 && j == 0)
                    continue;
                var t = layer.GetTile(x + i, y + j);
                Assert.True(t.IsExtension);
                Assert.Equal(big, t.ExtensionItemId);
                Assert.Equal(i, t.ExtensionX);
                Assert.Equal(j, t.ExtensionY);
            }
        }

        // Choosing one of its extra squares selects the whole item; removing it empties all of them.
        w.SelectTile(x + 3, y + 2);
        Assert.Equal(big, w.SelectedItem.ItemId);
        w.RemoveSelected();
        w.ApplyToSave();
        layer = MapTileManagerUtil.FromSaveFile(sav.Main).FieldItems.Layer0;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
                Assert.True(layer.GetTile(x + i, y + j).IsNone);
        }
        w.Close();
    }

    /// <summary>
    /// A planted tree goes on a whole square; something that doesn't fit is refused and nothing changes.
    /// </summary>
    [AvaloniaFact]
    public void MapPlantAndCollision()
    {
        var sav = Save();
        var (x, y) = FindEmptySquare(sav, 6);
        var w = new IslandMapWindow(sav);

        var tree = FieldItemList.Items.Values.First(d => d.Kind == FieldItemKind.PltTreeOak && !d.Name.Contains("Stump"));
        w.SelectTile(x, y);
        Assert.Equal(PlaceResult.Ok, w.PutItem(new Item((ushort)tree.Index)));

        // A 2x2 piece of furniture just left of the tree would cover it: refused.
        var big = GameInfo.Strings.ItemDataSource.Select(z => (ushort)z.Value)
            .First(id => id is > 0 and < 60_000 && ItemInfo.GetItemSize(id) == ItemSizeType.S_2_0x2_0 && ItemInfo.GetItemKind(id).IsFurniture);
        w.SelectTile(x - 2, y);
        Assert.True(w.SelectedItem.IsNone);
        Assert.Equal(PlaceResult.Collision, w.PutItem(new Item(big)));
        Assert.True(w.SelectedItem.IsNone);

        w.ApplyToSave();
        var layer = MapTileManagerUtil.FromSaveFile(sav.Main).FieldItems.Layer0;
        Assert.Equal((ushort)tree.Index, layer.GetTile(x, y).ItemId);
        Assert.True(layer.GetTile(x - 2, y).IsNone);
        w.Close();
    }

    /// <summary>
    /// Closing with Cancel leaves the save as it was.
    /// </summary>
    [AvaloniaFact]
    public void MapCancelKeepsTheSave()
    {
        var sav = Save();
        var (x, y) = FindEmptySquare(sav);
        var original = sav.Main.Data.ToArray();
        var w = new IslandMapWindow(sav);
        w.Show();
        w.SelectTile(x, y);
        w.PutItem(new Item(GameInfo.Strings.ItemDataSource.Select(z => (ushort)z.Value).First(id => id is > 0 and < 60_000 && ItemInfo.GetItemKind(id).IsFurniture)));
        w.Close();
        Assert.True(original.AsSpan().SequenceEqual(sav.Main.Data));
    }

    /// <summary>
    /// The painted ground of the whole map is quick enough to open the window without waiting.
    /// </summary>
    [AvaloniaFact]
    public void MapPaintTime()
    {
        var sav = Save();
        var map = MapTileManagerUtil.FromSaveFile(sav.Main);
        IslandPainter.PaintPixels(map); // warm up
        var sw = System.Diagnostics.Stopwatch.StartNew();
        IslandPainter.PaintPixels(map);
        Assert.True(sw.ElapsedMilliseconds < 3000, $"{sw.ElapsedMilliseconds} ms");
    }

    #region Houses
    private static (int X, int Y) FirstRoot(LayerItem layer, Func<Item, bool>? where = null)
    {
        for (int x = 0; x < layer.TileInfo.TotalWidth; x++)
        {
            for (int y = 0; y < layer.TileInfo.TotalHeight; y++)
            {
                var t = layer.GetTile(x, y);
                if (t.IsRoot && !t.IsNone && (where?.Invoke(t) ?? true))
                    return (x, y);
            }
        }
        throw new InvalidOperationException("Nothing in the layer");
    }

    [AvaloniaFact]
    public void HouseRooms()
    {
        var sav = Save();
        var w = new HouseWindow(sav) { Width = 1240, Height = 860 };
        w.Show();
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "55-house-main", false);

        var room = sav.Main.GetPlayerHouses()[0].GetRoom(0).GetItemLayers();
        var (x, y) = FirstRoot(room[(int)RoomLayerSurface.Floor], z => GridItems.CanRotate(z));
        w.ClickTile(x, y);
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "56-house-furniture", false);

        w.SetMode(RoomEditMode.Tables);
        (x, y) = FirstRoot(room[(int)RoomLayerSurface.FloorSupported]);
        w.ClickTile(x, y);
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "57-house-tables", false);

        w.SetMode(RoomEditMode.Walls);
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "58-house-walls", false);

        w.SetMode(RoomEditMode.Floor);
        w.ShowRoom(1);
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "59-house-back-room");
    }

    /// <summary>
    /// A piece of furniture in a room is changed for another, turned, and an empty spot gets a new one; the save gets them on saving.
    /// </summary>
    [AvaloniaFact]
    public void HouseChangeRoomItem()
    {
        var sav = Save();
        var floor = sav.Main.GetPlayerHouses()[0].GetRoom(0).GetItemLayers()[(int)RoomLayerSurface.Floor];
        var (x, y) = FirstRoot(floor, z => GridItems.GetSize(z) == (2, 2) && GridItems.CanRotate(z));
        var old = floor.GetTile(x, y).ItemId;

        var other = GameInfo.Strings.ItemDataSource.Select(z => (ushort)z.Value)
            .First(id => id is > 0 and < 60_000 && id != old && ItemInfo.GetItemSize(id) == ItemSizeType.S_1_0x1_0 && ItemInfo.GetItemKind(id).IsFurniture);

        var w = new HouseWindow(sav);
        w.ClickTile(x + 1, y + 1); // any of its tiles chooses it
        Assert.Equal(old, w.SelectedItem.ItemId);
        Assert.Equal(PlaceResult.Ok, w.PutItem(new Item(other)));
        Assert.Equal(other, w.SelectedItem.ItemId);
        var rotation = w.SelectedItem.Rotation;
        Assert.Equal(PlaceResult.Ok, w.RotateSelected());
        Assert.Equal((rotation + 1) & 3, w.SelectedItem.Rotation);

        // Not in the save until saving.
        Assert.Equal(old, sav.Main.GetPlayerHouses()[0].GetRoom(0).GetItemLayers()[0].GetTile(x, y).ItemId);
        w.ApplyToSave();
        var saved = sav.Main.GetPlayerHouses()[0].GetRoom(0).GetItemLayers()[0];
        Assert.Equal(other, saved.GetTile(x, y).ItemId);
        Assert.Equal((rotation + 1) & 3, saved.GetTile(x, y).Rotation);
        Assert.True(saved.GetTile(x + 1, y + 1).IsExtension);

        // Removing it empties all its tiles.
        w.ClickTile(x, y);
        w.RemoveSelected();
        w.ApplyToSave();
        saved = sav.Main.GetPlayerHouses()[0].GetRoom(0).GetItemLayers()[0];
        Assert.True(saved.GetTile(x, y).IsNone);
        Assert.True(saved.GetTile(x + 1, y + 1).IsNone);
        w.Close();
    }

    /// <summary>
    /// Things can't be placed outside the room's floor.
    /// </summary>
    [AvaloniaFact]
    public void HouseOutsideTheRoom()
    {
        var sav = Save();
        var w = new HouseWindow(sav);
        w.ShowRoom(1); // a 6x6 room: tiles 4 to 15
        w.ClickTile(0, 0);
        var item = GameInfo.Strings.ItemDataSource.Select(z => (ushort)z.Value)
            .First(id => id is > 0 and < 60_000 && ItemInfo.GetItemSize(id) == ItemSizeType.S_1_0x1_0 && ItemInfo.GetItemKind(id).IsFurniture);
        Assert.Equal(PlaceResult.OutOfBounds, w.PutItem(new Item(item)));
        w.ApplyToSave();
        Assert.True(sav.Main.GetPlayerHouses()[0].GetRoom(1).GetItemLayers()[0].GetTile(0, 0).IsNone);
        w.Close();
    }

    /// <summary>
    /// Closing the houses with Cancel leaves the save as it was.
    /// </summary>
    [AvaloniaFact]
    public void HouseCancelKeepsTheSave()
    {
        var sav = Save();
        var original = sav.Main.Data.ToArray();
        var w = new HouseWindow(sav);
        w.Show();
        var floor = sav.Main.GetPlayerHouses()[0].GetRoom(0).GetItemLayers()[0];
        var (x, y) = FirstRoot(floor);
        w.ClickTile(x, y);
        w.RemoveSelected();
        w.Close();
        Assert.True(original.AsSpan().SequenceEqual(sav.Main.Data));
    }

    [Fact]
    public void HouseStyleNamesArePlain()
    {
        Assert.DoesNotContain(HouseStyleNames.GetName(RoofType.HouseRoofNASlateD), "House");
        Assert.Contains("4", HouseStyleNames.GetName(RoofType.HouseRoofNASlateD));
        var names = HouseStyleNames.GetNames(Enum.GetValues<DoorKind>().Where(z => !z.ToString().StartsWith('_')).ToList());
        Assert.Equal(names.Count, names.Distinct().Count());
        Assert.All(names, n => Assert.DoesNotContain("HouseDoor", n));
    }
    #endregion

    /// <summary>
    /// The tidying tools: every weed of the island removed at once and back with undo; flowers removed only in a chosen
    /// area; a piece of furniture dragged elsewhere (60-…).
    /// </summary>
    [AvaloniaFact]
    public void MapTidyAndMove()
    {
        var sav = Save();
        var w = ShowMap(sav, out var view);
        Gallery.Capture(w, "60-map-tidy", false);

        var weeds = w.Count(IslandMapWindow.Tidy.Weeds);
        Assume(weeds > 0, "the test save has weeds");
        Assert.Equal(weeds, w.RemoveAll(IslandMapWindow.Tidy.Weeds));
        Assert.Equal(0, w.Count(IslandMapWindow.Tidy.Weeds));
        w.Undo();
        Assert.Equal(weeds, w.Count(IslandMapWindow.Tidy.Weeds));

        // Flowers only in an area: the ones outside stay.
        var flowers = w.Count(IslandMapWindow.Tidy.Flowers);
        var area = new Rect(0, 0, IslandMapView.MapWidth / 2, IslandMapView.MapHeight);
        var inside = w.Count(IslandMapWindow.Tidy.Flowers, area);
        w.ChooseArea(area);
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "60-map-area", false);
        Assert.Equal(inside, w.RemoveAll(IslandMapWindow.Tidy.Flowers, area));
        Assert.Equal(flowers - inside, w.Count(IslandMapWindow.Tidy.Flowers));

        // A piece of furniture dragged to an empty square.
        var map = MapTileManagerUtil.FromSaveFile(sav.Main);
        var (ox, oy) = map.ConfigItems.GetCoordinatesAbsolute();
        var (fx, fy) = FindEmptySquare(sav);
        var layer = map.FieldItems.Layer0;
        var piece = Enumerable.Range(0, layer.TileInfo.TotalWidth).SelectMany(x => Enumerable.Range(0, layer.TileInfo.TotalHeight).Select(y => (x, y)))
            .First(p => layer.GetTile(p.x, p.y) is { IsRoot: true, IsNone: false } t && !t.IsFieldItem && GridItems.GetSize(t) == (2, 2));
        Assert.Equal(PlaceResult.Ok, w.MoveItem(piece.x + ox, piece.y + oy, fx + ox, fy + oy));
        w.ApplyToSave();
        w.Close();
        var after = MapTileManagerUtil.FromSaveFile(sav.Main).FieldItems.Layer0;
        Assert.Equal(layer.GetTile(piece.x, piece.y).ItemId, after.GetTile(fx, fy).ItemId);
        Assert.True(after.GetTile(piece.x, piece.y).IsNone);
    }

    private static void Assume(bool condition, string what) => Assert.SkipUnless(condition, what);

    /// <summary> A piece of furniture dragged with the mouse lands where it was let go. </summary>
    [AvaloniaFact]
    public void MapDragWithMouse()
    {
        var sav = Save();
        var map = MapTileManagerUtil.FromSaveFile(sav.Main);
        var (ox, oy) = map.ConfigItems.GetCoordinatesAbsolute();
        var layer = map.FieldItems.Layer0;
        var (fx, fy) = FindEmptySquare(sav);
        var piece = Enumerable.Range(0, layer.TileInfo.TotalWidth).SelectMany(x => Enumerable.Range(0, layer.TileInfo.TotalHeight).Select(y => (x, y)))
            .First(p => layer.GetTile(p.x, p.y) is { IsRoot: true, IsNone: false } t && !t.IsFieldItem && GridItems.GetSize(t) == (2, 2)
                && Math.Abs(p.x - fx) < 30 && Math.Abs(p.y - fy) < 20);
        var w = ShowMap(sav, out var view);
        view.ShowArea(new Rect(Math.Min(piece.x, fx) + ox - 6, Math.Min(piece.y, fy) + oy - 6, Math.Abs(piece.x - fx) + 14, Math.Abs(piece.y - fy) + 14));
        Dispatcher.UIThread.RunJobs();
        Point At(double x, double y) => view.TranslatePoint(view.ToScreen(new Point(x + ox, y + oy)), w)!.Value;
        var from = At(piece.x + 1, piece.y + 1);
        var to = At(fx + 1, fy + 1);
        w.MouseMove(from, RawInputModifiers.None);
        w.MouseDown(from, MouseButton.Left, RawInputModifiers.None);
        w.MouseMove(to, RawInputModifiers.LeftMouseButton);
        w.MouseUp(to, MouseButton.Left, RawInputModifiers.None);
        w.ApplyToSave();
        w.Close();
        var after = MapTileManagerUtil.FromSaveFile(sav.Main).FieldItems.Layer0;
        Assert.Equal(layer.GetTile(piece.x, piece.y).ItemId, after.GetTile(fx, fy).ItemId);
        Assert.True(after.GetTile(piece.x, piece.y).IsNone);
    }

    /// <summary> The eraser takes only flowers along a stroke; a whole kind of flower goes at once; an area is marked without ⇧. </summary>
    [AvaloniaFact]
    public void MapEraserKindsAndSelect()
    {
        var sav = Save();
        var w = ShowMap(sav, out var view);
        var map = MapTileManagerUtil.FromSaveFile(sav.Main);
        var (ox, oy) = map.ConfigItems.GetCoordinatesAbsolute();
        var layer = map.FieldItems.Layer0;
        var height = layer.TileInfo.TotalHeight;
        var flower = Enumerable.Range(0, layer.Tiles.Length).Select(i => (I: i, T: layer.Tiles[i]))
            .FirstOrDefault(z => z.T.IsRoot && FieldItemList.Items.TryGetValue(z.T.ItemId, out var d) && d.Kind.IsFlower);
        Assume(flower.T is not null, "the test save has flowers");
        var (fx, fy) = (flower.I / height, flower.I % height);
        view.ShowArea(new Rect(fx + ox - 20, fy + oy - 12, 40, 24));
        Dispatcher.UIThread.RunJobs();
        Point At(double x, double y) => view.TranslatePoint(view.ToScreen(new Point(x + ox, y + oy)), w)!.Value;

        // The eraser along three squares: flowers go, nothing else.
        var flowers = w.Count(IslandMapWindow.Tidy.Flowers);
        var everything = w.Count(IslandMapWindow.Tidy.Everything);
        w.SetTool(MapTool.Erase);
        var from = At(fx + 1, fy + 1);
        var to = At(fx + 7, fy + 1);
        w.MouseMove(from, RawInputModifiers.None);
        w.MouseDown(from, MouseButton.Left, RawInputModifiers.None);
        w.MouseMove(to, RawInputModifiers.LeftMouseButton);
        Gallery.Capture(w, "60-map-eraser", false);
        w.MouseUp(to, MouseButton.Left, RawInputModifiers.None);
        var gone = flowers - w.Count(IslandMapWindow.Tidy.Flowers);
        Assert.True(gone >= 1);
        Assert.Equal(everything - gone, w.Count(IslandMapWindow.Tidy.Everything));
        w.Undo();
        Assert.Equal(flowers, w.Count(IslandMapWindow.Tidy.Flowers));

        // One kind of flower, on the whole island.
        var name = GridItems.GetName(flower.T);
        var kind = w.Kinds().Single(k => k.Name == name);
        Assert.Equal(kind.Count, w.RemoveNamed(new HashSet<string> { name }));
        Assert.DoesNotContain(w.Kinds(), k => k.Name == name);
        Assert.Equal(flowers - kind.Count, w.Count(IslandMapWindow.Tidy.Flowers));
        w.Undo();

        // The chosen flower offers to remove all of the same, or the eraser for them only.
        w.SetTool(MapTool.Move);
        w.SelectTile(fx, fy);
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "60-map-same", false);
        if (kind.Count > 1)
        {
            var buttons = w.GetVisualDescendants().OfType<Button>().ToList();
            Assert.Contains(buttons, b => Equals(b.Content, string.Format(MapStrings.RemoveSame, kind.Count)));
            var only = buttons.First(b => Equals(b.Content, string.Format(MapStrings.EraseThese, name)));
            only.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
            Dispatcher.UIThread.RunJobs();
            Assert.Equal(MapTool.Erase, view.Tool);
            var what = w.GetVisualDescendants().OfType<ComboBox>().First(c => c.SelectedItem is string t && t == string.Format(MapStrings.EraseOnly, name));
            Assert.NotNull(what);
        }

        // An area marked by dragging, without ⇧.
        w.SetTool(MapTool.Select);
        var a = At(fx - 6, fy - 6);
        var b2 = At(fx + 8, fy + 6);
        w.MouseMove(a, RawInputModifiers.None);
        w.MouseDown(a, MouseButton.Left, RawInputModifiers.None);
        w.MouseMove(b2, RawInputModifiers.LeftMouseButton);
        var marked = view.Area;
        w.MouseUp(b2, MouseButton.Left, RawInputModifiers.None);
        Dispatcher.UIThread.RunJobs();
        Assert.NotNull(marked);
        Assert.Equal(marked, view.Area); // the area stays after letting go (losing the capture used to forget it)
        Assert.Contains(w.GetVisualDescendants().OfType<Expander>(), e => e.IsExpanded && e.Header is string h && h.StartsWith(MapStrings.ByKind.Split('(')[0], StringComparison.Ordinal));
        Gallery.Capture(w, "60-map-select");
    }

    [AvaloniaFact]
    public void BridgeShapes()
    {
        Assert.Equal(("BridgeJapanese", 10, false), IslandMapWindow.BridgeShape(BridgeType.BridgeJapanese04));
        var diagonal = IslandMapWindow.BridgeShape(BridgeType.BridgeLogDiagonal030);
        Assert.True(diagonal.Diagonal);
        Assert.Equal("BridgeLog", diagonal.Material);
        Assert.True(diagonal.Length > 10);
    }
}
