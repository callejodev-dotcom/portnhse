using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// Moving furniture by dragging it and putting new things down in a room (59-…), checked in the save.
/// </summary>
public class GalleryHouseEdit
{
    private static HorizonSave Save()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        return TestSave.LoadCopy(out _);
    }

    private static LayerRoomItem Floor(HorizonSave sav) => sav.Main.GetPlayerHouses()[0].GetRoom(0).GetItemLayers()[(int)RoomLayerSurface.Floor];

    /// <summary>
    /// First free square of the floor of the main room (8×8 squares from tile 2), and a one-square piece of furniture.
    /// </summary>
    private static ((int X, int Y) Free, (int X, int Y) Small) Spots(LayerRoomItem floor)
    {
        (int, int)? free = null, small = null;
        for (int y = 2; y < 18; y += 2)
        {
            for (int x = 2; x < 18; x += 2)
            {
                var square = new[] { floor.GetTile(x, y), floor.GetTile(x + 1, y), floor.GetTile(x, y + 1), floor.GetTile(x + 1, y + 1) };
                if (free is null && square.All(z => z.IsNone))
                    free = (x, y);
            }
        }
        for (int y = 2; y < 18 && small is null; y++)
        {
            for (int x = 2; x < 18 && small is null; x++)
            {
                var t = floor.GetTile(x, y);
                if (t.IsRoot && !t.IsNone && GridItems.GetSize(t) == (2, 2))
                    small = (x, y);
            }
        }
        Assert.True(free is not null && small is not null, "the main room needs a free square and a one-square piece");
        return (free!.Value, small!.Value);
    }

    [AvaloniaFact]
    public void DragMovesFurniture()
    {
        var sav = Save();
        var (free, small) = Spots(Floor(sav));
        var id = Floor(sav).GetTile(small.X, small.Y).ItemId;

        var w = new HouseWindow(sav) { Width = 1240, Height = 860 };
        w.Show();
        Dispatcher.UIThread.RunJobs();
        Assert.Equal(PlaceResult.Ok, w.MoveItem(small.X, small.Y, free.X, free.Y));
        w.ApplyToSave();
        w.Close();

        var floor = Floor(sav);
        Assert.Equal(id, floor.GetTile(free.X, free.Y).ItemId);
        Assert.True(floor.GetTile(small.X, small.Y).IsNone);
    }

    [AvaloniaFact]
    public void DragOntoAnotherPieceIsRefused()
    {
        var sav = Save();
        var floor = Floor(sav);
        var (_, small) = Spots(floor);
        var other = Enumerable.Range(2, 16).SelectMany(y => Enumerable.Range(2, 16).Select(x => (x, y)))
            .First(p => p != small && floor.GetTile(p.x, p.y) is { IsRoot: true, IsNone: false });
        var before = floor.GetTile(other.x, other.y).ItemId;

        var w = new HouseWindow(sav) { Width = 1240, Height = 860 };
        w.Show();
        Dispatcher.UIThread.RunJobs();
        Assert.NotEqual(PlaceResult.Ok, w.MoveItem(small.X, small.Y, other.x, other.y));
        w.ApplyToSave();
        w.Close();

        floor = Floor(sav);
        Assert.Equal(before, floor.GetTile(other.x, other.y).ItemId);
        Assert.False(floor.GetTile(small.X, small.Y).IsNone);
    }

    [AvaloniaFact]
    public void PutNewThings()
    {
        var sav = Save();
        var (free, small) = Spots(Floor(sav));
        var chair = Floor(sav).GetTile(small.X, small.Y);

        var w = new HouseWindow(sav) { Width = 1240, Height = 860 };
        w.Show();
        Dispatcher.UIThread.RunJobs();
        w.StartPlacing(chair);
        Gallery.Capture(w, "59-house-placing", false);
        Assert.Equal(PlaceResult.Ok, w.PlaceNew(free.X, free.Y));
        Assert.Equal(PlaceResult.Collision, w.PlaceNew(free.X, free.Y));
        w.ApplyToSave();
        w.Close();

        var floor = Floor(sav);
        Assert.Equal(chair.ItemId, floor.GetTile(free.X, free.Y).ItemId);
        Assert.Equal(chair.ItemId, floor.GetTile(small.X, small.Y).ItemId);
    }
}

/// <summary>
/// Roofs, walls and doors chosen by their pictures (66-…). The pictures are only there when the test app data folder has
/// them (img/exterior); otherwise the captures show empty squares.
/// </summary>
public class GalleryHouseLooks
{
    [AvaloniaFact]
    public void Choices()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var sav = TestSave.LoadCopy(out _);
        var house = sav.Main.GetPlayerHouses()[0];

        var w = new HouseWindow(sav) { Width = 1240, Height = 1180 };
        Gallery.Capture(w, "66-house-looks");

        var roof = new HousePartPicker<RoofType>(HouseStyleNames.Offered<RoofType>("HouseRoofP"), house.RoofUniqueID);
        Assert.Equal(33, roof.Choice() is Avalonia.Controls.ScrollViewer { Content: Avalonia.Controls.WrapPanel p } ? p.Children.Count : 0);
        Gallery.Capture(new Avalonia.Controls.Window { Width = 600, Height = 520, Content = roof.Choice() }, "66-house-roofs");
        var walls = new HousePartPicker<WallType>(HouseStyleNames.Offered<WallType>("HouseWallP"), house.WallUniqueID);
        Gallery.Capture(new Avalonia.Controls.Window { Width = 600, Height = 520, Content = walls.Choice() }, "66-house-walls");
        var door = new DoorPicker(house.DoorUniqueID);
        Gallery.Capture(new Avalonia.Controls.Window { Width = 600, Height = 520, Content = door.Design.Choice() }, "66-house-doors");

        // Every look of the game list has a name or a picture key the core knows.
        foreach (var name in HouseStyleNames.Offered<RoofType>("HouseRoofP"))
            Assert.False(string.IsNullOrEmpty(HousePartPicker<RoofType>.NameOf(name)));
    }

    [AvaloniaFact]
    public void SquareDoorKeepsItsShape()
    {
        var square = Enum.GetValues<DoorKind>().First(z => z.ToString() is var n && n.StartsWith("HouseDoor") && n.EndsWith('S'));
        var picker = new DoorPicker(square);
        Assert.Equal(square, picker.Value);
        Assert.True(ExteriorPictures.Has(square.ToString()));
    }

    [AvaloniaFact]
    public void Previews()
    {
        (string, string, string)[] looks =
        [
            ("HouseRoofPA04StandardA", "HouseWallPA04StandardA", "HouseDoorVerticalWindowGR"),
            ("HouseRoofPA04StoneE", "HouseWallPB04BrickF", "HouseDoorStandardAR"),
            ("HouseRoofPA04ThatchedD", "HouseWallPC04JapaneseA", "HouseDoorChineseHR"),
            ("HouseRoofPA04WesterntileB", "HouseWallPD04SoilB", "HouseDoorSimpleBR"),
            ("HouseRoofPA03StandardC", "HouseWallPA03StandardA", "HouseDoorVerticalWindowGR"),
            ("HouseRoofNDStoneF", "HouseWallPB04LogD", "HouseDoorLatticeDR"),
        ];
        var grid = new Avalonia.Controls.WrapPanel { ItemSpacing = 10, LineSpacing = 10, Margin = new Avalonia.Thickness(10) };
        foreach (var (roof, walls, door) in looks)
        {
            var p = new HousePreview();
            p.Set(roof, walls, door);
            grid.Children.Add(p);
        }
        Gallery.Capture(new Avalonia.Controls.Window { Width = 1020, Height = 510, Content = grid }, "66-house-previews");
        Assert.Equal("Brick", HousePreview.Material("HouseWallPB04BrickF", "HouseWall"));
        Assert.Equal("Stucco", HousePreview.Material("HouseWallNDStuccoD", "HouseWall"));
    }
}
