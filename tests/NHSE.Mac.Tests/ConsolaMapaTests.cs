using System.Reflection;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The island map and the players' houses of NHSE for Switch (consola/source/core/mapa y casas): each change leaves the
/// whole save byte for byte as the Mac's map and house windows leave it (IslandMapWindow, HouseWindow, GridItems).
/// </summary>
public class ConsolaMapaTests
{
    private static readonly (int X, int Y) Origin = (0, 32); // where the item layer starts on the whole map

    /// <summary> Runs several orders of the Switch core on one copy, the same with NHSE on another, and compares. </summary>
    private static (HorizonSave Nhse, HorizonSave Switch) Many(Action<HorizonSave> nhse, params string[][] orders)
    {
        ConsolaTests.Skip();
        var sav = TestSave.LoadCopy(out var folder);
        foreach (var o in orders)
            ConsolaTests.Run(folder, o);
        var after = HorizonSave.FromFolder(folder);
        Assert.Empty(after.GetInvalidHashes());
        nhse(sav);
        sav.Main.Hash();
        ConsolaTests.SameBytes(sav.Main.Data, after.Main.Data);
        ConsolaTests.SameBytes(sav.Players[0].Personal.Data, after.Players[0].Personal.Data);
        return (sav, after);
    }

    private static string[] O(params object[] args) => [.. args.Select(z => Convert.ToString(z, System.Globalization.CultureInfo.InvariantCulture)!)];

    private static LayerFieldItem Ground(HorizonSave sav) => MapTileManagerUtil.FromSaveFile(sav.Main).FieldItems.Layer0;

    /// <summary> A free area of size × size tiles on the ground, away from the edges (layer tiles). </summary>
    private static (int X, int Y) FreeArea(int size = 8, int skip = 0)
    {
        ConsolaTests.Skip();
        var layer = Ground(TestSave.LoadCopy(out _));
        for (int y = 40; y < layer.TileInfo.TotalHeight - 40; y += 2)
        {
            for (int x = 60; x < layer.TileInfo.TotalWidth - 60; x += 2)
            {
                bool free = true;
                for (int i = 0; i < size && free; i++)
                    for (int j = 0; j < size && free; j++)
                        free = layer.GetTile(x + i, y + j).IsNone;
                if (free && skip-- == 0)
                    return (x, y);
            }
        }
        throw new InvalidOperationException("no free area");
    }

    private static ushort Furniture(ItemSizeType size, int skip = 0) => GameInfo.Strings.ItemDataSource.Select(z => (ushort)z.Value)
        .Where(id => id is > 0 and < 60_000 && ItemInfo.GetItemSize(id) == size && ItemInfo.GetItemKind(id).IsFurniture).Skip(skip).First();

    private static (int X, int Y) FirstRoot(LayerItem layer, Func<Item, bool> match)
    {
        var h = layer.TileInfo.TotalHeight;
        for (int i = 0; i < layer.Tiles.Length; i++)
        {
            var t = layer.Tiles[i];
            if (t.IsRoot && match(t))
                return (i / h, i % h);
        }
        throw new InvalidOperationException("not found");
    }

    private static void Map(HorizonSave sav, Action<IslandMapWindow> act)
    {
        var w = new IslandMapWindow(sav);
        act(w);
        w.ApplyToSave();
        w.Close();
    }

    #region Island
    [AvaloniaFact]
    public void BigFurnitureWithItsExtensions()
    {
        var (x, y) = FreeArea();
        var big = Furniture(ItemSizeType.S_2_0x2_0);
        var (_, after) = Many(sav => Map(sav, w =>
        {
            w.SelectTile(x, y);
            Assert.Equal(PlaceResult.Ok, w.PutItem(new Item(big)));
        }), O("mapa-poner", 0, x, y, big));
        Assert.True(Ground(after).GetTile(x + 3, y + 3).IsExtension);
    }

    [AvaloniaFact]
    public void ThingsLieOnTheGroundAndPlantsGoOnWholeSquares()
    {
        var (x, y) = FreeArea(8, 1);
        var tree = FieldItemList.Items.Values.First(d => d.Kind == FieldItemKind.PltTreeOak && !d.Name.Contains("Stump"));
        Many(sav => Map(sav, w =>
            {
                w.SelectTile(x, y);
                w.PutItem(new Item(2511) { Count = 29 }); // iron nuggets: dropped
                w.SelectTile(x + 3, y + 5);
                w.PutItem(new Item((ushort)tree.Index)); // a tree on an odd tile: on its square
            }),
            O("mapa-poner", 0, x, y, 2511, 29), O("mapa-poner", 0, x + 3, y + 5, tree.Index));
    }

    [AvaloniaFact]
    public void WhatDoesNotFitChangesNothing()
    {
        var (x, y) = FreeArea(10, 2);
        var tree = FieldItemList.Items.Values.First(d => d.Kind == FieldItemKind.PltTreeOak && !d.Name.Contains("Stump"));
        var big = Furniture(ItemSizeType.S_2_0x2_0);
        var (_, after) = Many(sav => Map(sav, w =>
            {
                w.SelectTile(x + 4, y);
                w.PutItem(new Item((ushort)tree.Index));
                w.SelectTile(x + 2, y);
                Assert.Equal(PlaceResult.Collision, w.PutItem(new Item(big)));
            }),
            O("mapa-poner", 0, x + 4, y, tree.Index), O("mapa-poner", 0, x + 2, y, big));
        Assert.True(Ground(after).GetTile(x + 2, y).IsNone);
    }

    [AvaloniaFact]
    public void TurnMoveAndRemove()
    {
        var (x, y) = FreeArea(16, 3);
        var wide = Furniture(ItemSizeType.S_3_0x2_0);
        var other = Furniture(ItemSizeType.S_1_0x1_0);
        Many(sav => Map(sav, w =>
            {
                w.SelectTile(x, y);
                w.PutItem(new Item(wide));
                Assert.Equal(PlaceResult.Ok, w.RotateSelected());
                Assert.Equal(PlaceResult.Ok, w.MoveItem(x + Origin.X, y + Origin.Y, x + 6 + Origin.X, y + 4 + Origin.Y));
                w.SelectTile(x + 12, y + 12);
                w.PutItem(new Item(other));
                w.SelectTile(x + 13, y + 13); // one of its extra tiles
                w.RemoveSelected();
            }),
            O("mapa-poner", 0, x, y, wide), O("mapa-girar", 0, x + 1, y + 1), O("mapa-mover", 0, x, y, x + 6, y + 4),
            O("mapa-poner", 0, x + 12, y + 12, other), O("mapa-quitar", 0, x + 13, y + 13));
    }

    [AvaloniaFact]
    public void MovingOntoSomethingIsRefused()
    {
        var (x, y) = FreeArea(12, 4);
        var a = Furniture(ItemSizeType.S_2_0x2_0);
        var b = Furniture(ItemSizeType.S_1_0x1_0, 3);
        Many(sav => Map(sav, w =>
            {
                w.SelectTile(x, y);
                w.PutItem(new Item(a));
                w.SelectTile(x + 6, y);
                w.PutItem(new Item(b));
                Assert.Equal(PlaceResult.Collision, w.MoveItem(x + Origin.X, y + Origin.Y, x + 4 + Origin.X, y + Origin.Y));
            }),
            O("mapa-poner", 0, x, y, a), O("mapa-poner", 0, x + 6, y, b), O("mapa-mover", 0, x, y, x + 4, y));
    }

    [AvaloniaFact]
    public void TheLayerOnTop()
    {
        var (x, y) = FreeArea(8, 5);
        var small = Furniture(ItemSizeType.S_1_0x1_0, 5);
        Many(sav => Map(sav, w =>
            {
                typeof(IslandMapWindow).GetMethod("SetLayer", BindingFlags.NonPublic | BindingFlags.Instance)!.Invoke(w, [1]);
                w.SelectTile(x, y);
                w.PutItem(new Item(2511) { Count = 4 }); // not dropped on the top layer
                w.SelectTile(x + 4, y);
                w.PutItem(new Item(small));
            }),
            O("mapa-poner", 1, x, y, 2511, 4), O("mapa-poner", 1, x + 4, y, small));
    }

    [AvaloniaFact]
    public void RemoveEveryWeed() => Many(sav => Map(sav, w => w.RemoveAll(IslandMapWindow.Tidy.Weeds)), O("mapa-quitar-tipo", 0, "hierbas"));

    [AvaloniaFact]
    public void RemoveInAnArea()
    {
        Rect area = new(100, 60 + Origin.Y, 60, 50);
        Many(sav => Map(sav, w =>
            {
                w.RemoveAll(IslandMapWindow.Tidy.Flowers, area);
                w.RemoveAll(IslandMapWindow.Tidy.Things, area);
                w.RemoveAll(IslandMapWindow.Tidy.Trees, area);
            }),
            O("mapa-quitar-tipo", 0, "flores", 100, 60, 60, 50), O("mapa-quitar-tipo", 0, "cosas", 100, 60, 60, 50),
            O("mapa-quitar-tipo", 0, "arboles", 100, 60, 60, 50));
    }

    [AvaloniaFact]
    public void RemoveAllOfTheSameName()
    {
        ConsolaTests.Skip();
        var layer = Ground(TestSave.LoadCopy(out _));
        var (x, y) = FirstRoot(layer, t => FieldItemList.Items.TryGetValue(t.ItemId, out var d) && d.Kind.IsFlower);
        var (fx, fy) = FirstRoot(layer, t => !FieldItemList.Items.ContainsKey(t.ItemId));
        Many(sav => Map(sav, w =>
            {
                var g = Ground(sav);
                w.RemoveNamed(new HashSet<string> { GridItems.GetName(g.GetTile(x, y)) });
                w.RemoveNamed(new HashSet<string> { GridItems.GetName(g.GetTile(fx, fy)) });
            }),
            O("mapa-quitar-iguales", 0, x, y), O("mapa-quitar-iguales", 0, fx, fy));
    }

    [AvaloniaFact]
    public void ReplaceAndWaterFlowers()
    {
        Rect area = new(80, 40 + Origin.Y, 120, 100);
        ushort[] ids = [0xEBED, 0xEBE4];
        Many(sav => Map(sav, w =>
            {
                w.ReplaceAll(IslandMapWindow.Tidy.Flowers, ids, false, area);
                w.WaterFlowers();
                w.ReplaceAll(IslandMapWindow.Tidy.Bushes, [ids[0]], false);
            }),
            O("mapa-cambiar", 0, "flores", "EBED,EBE4", 80, 40, 120, 100), O("mapa-regar", 0), O("mapa-cambiar", 0, "arbustos", "EBED"));
    }

    [AvaloniaFact]
    public void TheEraser()
    {
        ConsolaTests.Skip();
        var layer = Ground(TestSave.LoadCopy(out _));
        var (tx, ty) = FirstRoot(layer, t => FieldItemList.Items.TryGetValue(t.ItemId, out var d) && d.Kind.IsTree);
        var (fx, fy) = FirstRoot(layer, t => FieldItemList.Items.TryGetValue(t.ItemId, out var d) && d.Kind.IsFlower);
        var (ox, oy) = FirstRoot(layer, t => !FieldItemList.Items.ContainsKey(t.ItemId));
        Many(sav => Map(sav, w =>
            {
                var what = (ComboBox)typeof(IslandMapWindow).GetField("EraseWhat", BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(w)!;
                what.SelectedIndex = 9; // everything
                w.Erase(new Rect(tx + 1 + Origin.X, ty + 1 + Origin.Y, 2, 2)); // half a tree: all of it
                what.SelectedIndex = 0; // flowers
                w.Erase(new Rect(fx - 10 + Origin.X, fy - 10 + Origin.Y, 30, 30));
                // Only one name (as «Goma solo para…»)
                var choices = typeof(IslandMapWindow).GetMethod("EraseOnly", BindingFlags.NonPublic | BindingFlags.Instance)!;
                var name = GridItems.GetName(layer.GetTile(ox, oy));
                choices.Invoke(w, [name, new HashSet<string> { name }]);
                w.Erase(new Rect(ox - 8 + Origin.X, oy - 8 + Origin.Y, 20, 20));
            }),
            O("mapa-goma", 0, "todo", tx + 1, ty + 1, 2, 2), O("mapa-goma", 0, "flores", fx - 10, fy - 10, 30, 30),
            O("mapa-goma", 0, "todo", ox - 8, oy - 8, 20, 20, ox, oy));
    }
    [Fact]
    public void ThingsOnTopOfNothing()
    {
        // What the Mac's map asks about before saving (GetUnsupportedTiles): something on the top layer with nothing under it.
        var (x, y) = FreeArea(8, 6);
        ConsolaTests.Skip();
        TestSave.LoadCopy(out var folder);
        ConsolaTests.Run(folder, O("mapa-poner", 1, x, y, 2511, 4));
        var after = HorizonSave.FromFolder(folder);
        var expected = ((ILayerFieldItemSet)MapTileManagerUtil.FromSaveFile(after.Main).FieldItems).GetUnsupportedTiles();
        Assert.NotEmpty(expected);
        var output = ConsolaTests.Run(folder, O("mapa-sin-apoyo"));
        Assert.Contains($"sin apoyo {expected.Count}\n", output);
        foreach (var t in expected)
            Assert.Contains(t + "\n", output);
    }
    #endregion

    #region Houses
    private static void House(HorizonSave sav, Action<HouseWindow> act)
    {
        var w = new HouseWindow(sav);
        act(w);
        w.ApplyToSave();
        w.Close();
    }

    private static LayerRoomItem RoomLayer(HorizonSave sav, int room, int layer) => sav.Main.GetPlayerHouses()[0].GetRoom(room).GetItemLayers()[layer];

    /// <summary> A free square area of the room's floor (room tiles). </summary>
    private static (int X, int Y) FreeInRoom(LayerRoomItem l, int size, int x0, int y0, int x1, int y1)
    {
        for (int x = x0; x + size <= x1; x += 2)
            for (int y = y0; y + size <= y1; y += 2)
            {
                bool free = true;
                for (int i = 0; i < size && free; i++)
                    for (int j = 0; j < size && free; j++)
                        free = l.GetTile(x + i, y + j).IsNone;
                if (free)
                    return (x, y);
            }
        throw new InvalidOperationException("no room");
    }

    [AvaloniaFact]
    public void HouseFurniturePutTurnedMovedAndRemoved()
    {
        ConsolaTests.Skip();
        var floor = RoomLayer(TestSave.LoadCopy(out _), 0, 0);
        var (x, y) = FreeInRoom(floor, 4, 2, 2, 18, 18);
        var (rx, ry) = FirstRoot(floor, t => GridItems.CanRotate(t));
        var small = Furniture(ItemSizeType.S_1_0x1_0, 7);
        Many(sav => House(sav, w =>
            {
                w.ClickTile(x, y);
                Assert.Equal(PlaceResult.Ok, w.PutItem(new Item(small)));
                w.RotateSelected();
                w.ClickTile(rx, ry);
                w.RotateSelected();
                w.MoveItem(x, y, x + 2, y + 2);
                w.ClickTile(rx, ry);
                w.RemoveSelected();
            }),
            O("casa-poner", 0, 0, 0, x, y, small), O("casa-girar", 0, 0, 0, x, y), O("casa-girar", 0, 0, 0, rx, ry),
            O("casa-mover", 0, 0, 0, x, y, x + 2, y + 2), O("casa-quitar", 0, 0, 0, rx, ry));
    }

    [AvaloniaFact]
    public void HouseOutsideTheRoomAndNewThings()
    {
        var small = Furniture(ItemSizeType.S_1_0x1_0, 2);
        Many(sav => House(sav, w =>
            {
                w.ShowRoom(1); // 6 × 6 squares: tiles 4 to 15
                w.ClickTile(0, 0);
                Assert.Equal(PlaceResult.OutOfBounds, w.PutItem(new Item(small)));
                w.StartPlacing(new Item(small));
                w.PlaceNew(14, 14); // sticks out
                w.PlaceNew(4, 14);
            }),
            O("casa-poner", 0, 1, 0, 0, 0, small), O("casa-nuevo", 0, 1, 0, 14, 14, small), O("casa-nuevo", 0, 1, 0, 4, 14, small));
    }

    [AvaloniaFact]
    public void HouseWallsRugsAndCeiling()
    {
        var poster = GameInfo.Strings.ItemDataSource.Select(z => (ushort)z.Value)
            .First(id => id is > 0 and < 60_000 && ItemInfo.GetItemKind(id) == ItemKind.Kind_Poster);
        var rug = GameInfo.Strings.ItemDataSource.Select(z => (ushort)z.Value)
            .First(id => id is > 0 and < 60_000 && ItemInfo.GetItemKind(id) == ItemKind.Kind_Rug && ItemInfo.GetItemSize(id) == ItemSizeType.S_2_0x1_0_Rug);
        Many(sav => House(sav, w =>
            {
                w.ShowRoom(2);
                w.SetMode(RoomEditMode.Walls);
                w.SetWall(RoomLayerSurface.WallWest);
                w.ClickTile(4, 2);
                w.PutItem(new Item(poster));
                w.SetMode(RoomEditMode.Rugs);
                w.ClickTile(14, 4);
                w.PutItem(new Item(rug)); // sticks out
                w.ClickTile(6, 8);
                w.PutItem(new Item(rug));
            }),
            O("casa-poner", 0, 2, 3, 4, 2, poster), O("casa-poner", 0, 2, 6, 14, 4, rug), O("casa-poner", 0, 2, 6, 6, 8, rug));
    }

    [Fact]
    public void HouseSizeLooksAndWallpaper()
    {
        var paper = GameInfo.Strings.ItemDataSource.Select(z => (ushort)z.Value).First(id => id is > 0 and < 60_000 && ItemInfo.GetItemKind(id) == ItemKind.Kind_RoomWall);
        var floor = GameInfo.Strings.ItemDataSource.Select(z => (ushort)z.Value).First(id => id is > 0 and < 60_000 && ItemInfo.GetItemKind(id) == ItemKind.Kind_RoomFloor);
        Many(sav =>
            {
                // What the Mac's house window does with the size, roof, walls and door, and the room's wallpaper and flooring.
                var houses = sav.Main.GetPlayerHouses();
                var house = houses[0];
                house.HouseLevel = 8;
                house.RoofUniqueID = house.OrderRoofUniqueID = RoofType.HouseRoofPB04SlateA;
                house.WallUniqueID = house.OrderWallUniqueID = (WallType)5;
                house.DoorUniqueID = house.OrderDoorUniqueID = (DoorKind)7;
                var room = (PlayerRoom1)house.GetRoom(3);
                var fw = room.RoomFloorWall;
                var w = default(GSaveItemName);
                w.CopyFrom(new Item(paper));
                fw.WallItem = w;
                var f = default(GSaveItemName);
                f.CopyFrom(new Item(floor));
                fw.FloorItem = f;
                room.RoomFloorWall = fw;
                house.SetRoom(3, room);
                sav.Main.SetPlayerHouses(houses);
            },
            O("casa-nivel", 0, 8), O("casa-aspecto", 0, 0, (int)RoofType.HouseRoofPB04SlateA), O("casa-aspecto", 0, 1, 5), O("casa-aspecto", 0, 2, 7),
            O("casa-pared", 0, 3, 0, paper), O("casa-pared", 0, 3, 1, floor));
    }
    #endregion

    #region House advanced options
    /// <summary> Like <see cref="Many"/>, for orders that need files: NHSE goes first (it writes the files the orders read). </summary>
    private static void WithFiles(Action<HorizonSave, string> nhse, Func<string, string[][]> orders)
    {
        ConsolaTests.Skip();
        var sav = TestSave.LoadCopy(out var folder);
        var files = Path.Combine(Path.GetTempPath(), "nhse-casas-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(files);
        try
        {
            nhse(sav, files);
            foreach (var o in orders(files))
                ConsolaTests.Run(folder, o);
            var after = HorizonSave.FromFolder(folder);
            Assert.Empty(after.GetInvalidHashes());
            sav.Main.Hash();
            ConsolaTests.SameBytes(sav.Main.Data, after.Main.Data);
        }
        finally
        {
            Directory.Delete(files, true);
        }
    }

    [Fact]
    public void HouseEventsAndLooseValues()
    {
        var deco = GameInfo.Strings.ItemDataSource.Select(z => (ushort)z.Value).First(id => id is > 100 and < 60_000 && ItemInfo.GetItemKind(id).IsFurniture);
        Many(sav =>
            {
                // What the Mac's «Advanced options» do: the house's event flags (PlayerHouseFlagEditor) and the
                // PropertyGrid on the house (each property set as the grid sets it).
                var houses = sav.Main.GetPlayerHouses();
                var h = (PlayerHouse2)houses[0];
                h.EventFlags[1] = 1;
                h.EventFlags[11] = 25;
                h.EventFlags[100] = -3;
                var loan = h.Loan;
                loan.Value = 98000;
                h.Loan = loan;
                var unknown = h.Unknown;
                unknown.Value = 7;
                h.Unknown = unknown;
                h.NPC1 = -1;
                h.NPC2 = 3;
                h.PlayerHouseFlag = true;
                h.OrderWallUniqueID = (WallType)5;
                h.OrderRoofUniqueID = (RoofType)7;
                h.OrderDoorUniqueID = (DoorKind)9;
                h.HouseLevel = 6;
                var item = h.DoorDecoItemName;
                item.ItemId = deco;
                item.Count = 2;
                h.DoorDecoItemName = item;
                h.PostItemName = new Item(deco) { SystemParam = 0x20 };
                h.OrderPostItemName = new Item(Item.NONE);
                sav.Main.SetPlayerHouses(houses);
            },
            O("casa-suceso", 0, 1, 1), O("casa-suceso", 0, 11, 25), O("casa-suceso", 0, 100, -3),
            O("casa-valor", 0, 7, 98000), O("casa-valor", 0, 8, 7), O("casa-valor", 0, 9, -1), O("casa-valor", 0, 10, 3),
            O("casa-valor", 0, 11, 1), O("casa-valor", 0, 4, 5), O("casa-valor", 0, 5, 7), O("casa-valor", 0, 6, 9), O("casa-valor", 0, 0, 6),
            O("casa-objeto", 0, 0, deco, 2), O("casa-objeto", 0, 1, deco, 0, 0x20), O("casa-objeto", 0, 2, Item.NONE));
    }

    [Fact]
    public void HouseEventsFromAFile() => WithFiles((sav, dir) =>
        {
            // PlayerHouseFlagEditor's «Load»: a .nhfl of 256 bytes over the flags.
            var data = new byte[256];
            for (int i = 0; i < data.Length; i++)
                data[i] = (byte)(i * 7);
            File.WriteAllBytes(Path.Combine(dir, "EventFlagHouse.nhfl"), data);
            var houses = sav.Main.GetPlayerHouses();
            var edit = houses[0].EventFlags.ToArray();
            Buffer.BlockCopy(data, 0, edit, 0, data.Length);
            edit.CopyTo(houses[0].EventFlags);
            sav.Main.SetPlayerHouses(houses);
        }, dir => [O("casa-sucesos-importar", 0, Path.Combine(dir, "EventFlagHouse.nhfl"))]);

    [Fact]
    public void HouseAndRoomExportedAsNhse()
    {
        ConsolaTests.Skip();
        var sav = TestSave.LoadCopy(out var folder);
        var dir = Path.Combine(Path.GetTempPath(), "nhse-casas-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        try
        {
            ConsolaTests.Run(folder, O("casa-jugador-exportar", 0, Path.Combine(dir, "casa.nhph2")));
            ConsolaTests.Run(folder, O("casa-hab-exportar", 0, 2, Path.Combine(dir, "hab.nhpr2")));
            ConsolaTests.Run(folder, O("casa-sucesos-exportar", 0, Path.Combine(dir, "sucesos.nhfl")));
            var house = sav.Main.GetPlayerHouses()[0];
            ConsolaTests.SameBytes(house.Write(), File.ReadAllBytes(Path.Combine(dir, "casa.nhph2")));
            ConsolaTests.SameBytes(house.GetRoom(2).Write(), File.ReadAllBytes(Path.Combine(dir, "hab.nhpr2")));
            var flags = house.EventFlags.ToArray();
            var bytes = new byte[flags.Length * 2];
            Buffer.BlockCopy(flags, 0, bytes, 0, bytes.Length); // PlayerHouseFlagEditor's «Dump»
            ConsolaTests.SameBytes(bytes, File.ReadAllBytes(Path.Combine(dir, "sucesos.nhfl")));
        }
        finally
        {
            Directory.Delete(dir, true);
        }
    }

    /// <summary> MiscDumpHelper.LoadHouse without the file dialog. </summary>
    private static void LoadHouse(HorizonSave sav, IPlayerHouse[] houses, int index, string path)
    {
        var offsets = sav.Main.Offsets;
        var expectLength = offsets.PlayerHouseSize;
        var data = File.ReadAllBytes(path);
        Assert.True(PlayerHouseConverter.IsCompatible(data.Length, expectLength));
        data = PlayerHouseConverter.GetCompatible(data, expectLength);
        var h = offsets.ReadPlayerHouse(data);
        var current = houses[index];
        h.NPC1 = current.NPC1;
        houses[index] = h;
    }

    /// <summary> MiscDumpHelper.LoadRoom without the file dialog. </summary>
    private static IPlayerRoom LoadRoom(HorizonSave sav, string path)
    {
        var offsets = sav.Main.Offsets;
        var data = File.ReadAllBytes(path);
        Assert.True(PlayerRoomConverter.IsCompatible(data.Length, offsets.PlayerRoomSize));
        data = PlayerRoomConverter.GetCompatible(data, offsets.PlayerRoomSize);
        return offsets.ReadPlayerRoom(data);
    }

    [Fact]
    public void HouseImportedAsNhse() => WithFiles((sav, dir) =>
        {
            // A changed copy of the first house, in the new and the old format, into the second and third houses.
            var houses = sav.Main.GetPlayerHouses();
            var source = (PlayerHouse2)sav.Main.GetPlayerHouses()[0];
            source.HouseLevel = 5;
            source.Data[0x263D0] ^= 0x5A;
            source.NPC1 = 4;
            File.WriteAllBytes(Path.Combine(dir, "nueva.nhph2"), source.Write());
            File.WriteAllBytes(Path.Combine(dir, "vieja.nhph"), source.Downgrade().Write());
            LoadHouse(sav, houses, 1, Path.Combine(dir, "nueva.nhph2"));
            LoadHouse(sav, houses, 2, Path.Combine(dir, "vieja.nhph"));
            sav.Main.SetPlayerHouses(houses);
        }, dir => [O("casa-jugador-importar", 1, Path.Combine(dir, "nueva.nhph2")), O("casa-jugador-importar", 2, Path.Combine(dir, "vieja.nhph"))]);

    [Fact]
    public void RoomImportedAsNhse() => WithFiles((sav, dir) =>
        {
            var houses = sav.Main.GetPlayerHouses();
            var room = (PlayerRoom2)houses[0].GetRoom(0);
            room.Data[0x6C10] = 0x33; // past the old format: lost in the old file
            File.WriteAllBytes(Path.Combine(dir, "hab.nhpr2"), room.Write());
            File.WriteAllBytes(Path.Combine(dir, "vieja.nhpr"), room.Downgrade().Write());
            houses[0].SetRoom(3, LoadRoom(sav, Path.Combine(dir, "hab.nhpr2")));
            houses[0].SetRoom(4, LoadRoom(sav, Path.Combine(dir, "vieja.nhpr")));
            sav.Main.SetPlayerHouses(houses);
        }, dir => [O("casa-hab-importar", 0, 3, Path.Combine(dir, "hab.nhpr2")), O("casa-hab-importar", 0, 4, Path.Combine(dir, "vieja.nhpr"))]);
    #endregion
}
