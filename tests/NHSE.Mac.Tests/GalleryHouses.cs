using Avalonia;
using Avalonia.Controls;
using Avalonia.Headless;
using Avalonia.Headless.XUnit;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// Houses, island and design windows: screenshots in galeria/ and a few checks of their logic.
/// </summary>
public class GalleryHouses
{
    private static HorizonSave Save()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        return TestSave.LoadCopy(out _);
    }

    [AvaloniaFact]
    public void VillagerHouses()
    {
        var sav = Save();
        var houses = sav.Main.GetVillagerHouses();
        Gallery.Capture(new VillagerHouseEditor(houses, sav.Main.GetVillagers(), sav.Main, 0), "30-villager-houses");
    }

    [AvaloniaFact]
    public void PlayerHouses()
    {
        var sav = Save();
        var houses = sav.Main.GetPlayerHouses();
        Gallery.Capture(new PlayerHouseEditor(houses, sav.Players, sav.Main, 0), "31-player-houses");
    }

    [AvaloniaFact]
    public void PlayerHouseFlags()
    {
        var sav = Save();
        var flags = sav.Main.GetPlayerHouses()[0].EventFlags.ToArray();
        Gallery.Capture(new PlayerHouseFlagEditor(flags), "32-player-house-flags");
    }

    [AvaloniaFact]
    public void Campsite()
    {
        var sav = Save();
        Gallery.Capture(new CampsiteEditor(sav), "33-campsite");

        // With a visitor (the first villager of the island), and locked.
        var v = sav.Main.GetVillagers()[0];
        sav.Main.CampsiteStatus = 3;
        sav.Main.CampsiteVillagerSpeciesID = v.Species;
        sav.Main.CampsiteVillagerVariantID = v.Variant;
        Gallery.Capture(new CampsiteEditor(sav), "33-campsite-visitor");
        sav.Main.CampsiteStatus = 1;
        Gallery.Capture(new CampsiteEditor(sav), "33-campsite-locked");
    }

    [AvaloniaFact]
    public void FruitsFlowers()
    {
        var sav = Save();
        Gallery.Capture(new FruitsFlowersEditor(sav.Players[0], sav.Main), "34-fruits-flowers");
    }

    [AvaloniaFact]
    public void Museum()
    {
        var sav = Save();
        Gallery.Capture(new MuseumEditor(sav.Main.Museum), "35-museum");
    }

    [AvaloniaFact]
    public void Patterns()
    {
        var sav = Save();
        Gallery.Capture(new PatternEditor(sav.Main.GetDesigns(), sav.Players[0]), "36-patterns");
    }

    [AvaloniaFact]
    public void PatternsPRO()
    {
        var sav = Save();
        Gallery.Capture(new PatternEditorPRO(sav.Main.GetDesignsPRO(), sav.Players[0]), "37-patterns-pro");
    }

    /// <summary>
    /// The .png saved from the pattern preview has the pattern's pixels, enlarged without blending.
    /// </summary>
    [AvaloniaFact]
    public void PatternPngExport()
    {
        var sav = Save();
        var patterns = sav.Main.GetDesigns();
        var w = new PatternEditor(patterns, sav.Players[0]);
        w.Show();

        var bmp = Assert.IsAssignableFrom<Bitmap>(w.FindControl<Image>("PB_Pattern")!.Source);
        var path = Path.Combine(Path.GetTempPath(), $"nhse-pattern-{Guid.NewGuid():N}.png");
        try
        {
            bmp.Save(path, new PngBitmapEncoderOptions());
            const int size = DesignPattern.Width * 8;
            int[] saved;
            using (var fs = File.OpenRead(path))
                saved = ImageUtil.GetPixels(fs, size, size);

            using var original = patterns[0].GetImage();
            var expect = ImageUtil.ScalePixelImage(original.GetPixels(), 8, DesignPattern.Width, DesignPattern.Height, out _, out _);
            Assert.Equal(expect.Length, saved.Length);
            for (int i = 0; i < expect.Length; i++)
            {
                if ((uint)expect[i] >> 24 == 0)
                    Assert.Equal(0u, (uint)saved[i] >> 24); // transparent pixel
                else
                    Assert.Equal(expect[i], saved[i]);
            }
        }
        finally
        {
            File.Delete(path);
            w.Close();
        }
    }

    /// <summary>
    /// A design dumped to .nhd and loaded into another slot replaces it and its name in the list.
    /// </summary>
    [AvaloniaFact]
    public async Task PatternNhdImport()
    {
        var sav = Save();
        var patterns = sav.Main.GetDesigns();
        var player = sav.Players[0];
        // Another design, made by the player so that the import doesn't ask about its origin.
        var source = new DesignPattern(patterns[1].Data.ToArray());
        source.ChangeOrigins(player.Personal, source.Data);

        var path = Path.Combine(Path.GetTempPath(), $"nhse-pattern-{Guid.NewGuid():N}.nhd");
        await File.WriteAllBytesAsync(path, source.Data.ToArray());
        var w = new PatternEditor(patterns, player);
        w.Show();
        try
        {
            Assert.True(await w.ImportDesign(path));
            Assert.Equal(source.DesignName, patterns[0].DesignName);
            Assert.True(patterns[0].GetBitmap().AsSpan().SequenceEqual(source.GetBitmap()));
            Assert.Equal(source.DesignName, w.FindControl<ListBox>("LB_Items")!.Items[0]);
        }
        finally
        {
            File.Delete(path);
            w.Close();
        }
    }

    /// <summary>
    /// Same for the PRO designs (.nhpd).
    /// </summary>
    [AvaloniaFact]
    public async Task PatternPROImport()
    {
        var sav = Save();
        var patterns = sav.Main.GetDesignsPRO();
        var player = sav.Players[0];
        var source = new DesignPatternPRO(patterns[1].Data.ToArray());
        source.ChangeOrigins(player.Personal, source.Data);

        var path = Path.Combine(Path.GetTempPath(), $"nhse-pattern-{Guid.NewGuid():N}.nhpd");
        await File.WriteAllBytesAsync(path, source.Data.ToArray());
        var w = new PatternEditorPRO(patterns, player);
        w.Show();
        try
        {
            Assert.True(await w.ImportDesign(path));
            Assert.Equal(source.DesignName, patterns[0].DesignName);
            for (int sheet = 0; sheet < DesignPatternPRO.SheetCount; sheet++)
                Assert.True(patterns[0].GetBitmap(sheet).AsSpan().SequenceEqual(source.GetBitmap(sheet)));
        }
        finally
        {
            File.Delete(path);
            w.Close();
        }
    }

    private static void Click(Window w, string button) => w.FindControl<Button>(button)!.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));

    /// <summary>
    /// Editing a value renames its row and keeps it selected.
    /// </summary>
    [AvaloniaFact]
    public void PlayerHouseFlagsEdit()
    {
        var sav = Save();
        var flags = sav.Main.GetPlayerHouses()[0].EventFlags.ToArray();
        var w = new PlayerHouseFlagEditor(flags);
        w.Show();
        var list = w.FindControl<ListBox>("LB_Counts")!;
        list.SelectedIndex = 9;
        w.FindControl<NumericUpDown>("NUD_Count")!.Value = 1234;
        Dispatcher.UIThread.RunJobs();

        Assert.Equal(1234, flags[9]);
        Assert.Equal(9, list.SelectedIndex);
        Assert.EndsWith("= 1234", (string)list.Items[9]!);
        w.Close();
    }

    /// <summary>
    /// ⌥ click on a room tile deletes its item, and Save writes the room back to the house.
    /// </summary>
    [AvaloniaFact]
    public void PlayerHouseRoomDelete()
    {
        var sav = Save();
        var houses = sav.Main.GetPlayerHouses();
        var layer = houses[0].GetRoom(0).GetItemLayers()[0];
        var (x, y) = FindRoot(layer);
        Assert.SkipWhen(x < 0, "The first room of the first house is empty");

        var w = new PlayerHouseEditor(houses, sav.Players, sav.Main, 0);
        w.Show();
        Dispatcher.UIThread.RunJobs();
        var pb = w.FindControl<Image>("PB_Room")!;
        var p = pb.TranslatePoint(new Point((x * 24) + 12, (y * 24) + 12), w)!.Value;
        w.MouseMove(p, RawInputModifiers.None);
        Assert.StartsWith($"({x:000},{y:000})", w.FindControl<TextBlock>("L_Coordinates")!.Text);
        w.MouseDown(p, MouseButton.Left, RawInputModifiers.Alt);
        w.MouseUp(p, MouseButton.Left, RawInputModifiers.Alt);
        Click(w, "B_Save");

        Assert.True(houses[0].GetRoom(0).GetItemLayers()[0].GetTile(x, y).IsNone);
    }

    private static (int X, int Y) FindRoot(LayerItem layer)
    {
        for (int x = 0; x < layer.TileInfo.TotalWidth; x++)
        {
            for (int y = 0; y < layer.TileInfo.TotalHeight; y++)
            {
                var t = layer.GetTile(x, y);
                if (!t.IsNone && t.IsRoot)
                    return (x, y);
            }
        }
        return (-1, -1);
    }

    /// <summary>
    /// Save keeps the changes of the selected donation too.
    /// </summary>
    [AvaloniaFact]
    public void MuseumSaveSelected()
    {
        var sav = Save();
        var museum = sav.Main.Museum;
        var w = new MuseumEditor(museum);
        w.Show();
        var player = (byte)(museum.GetPlayers()[0] ^ 1);
        w.FindControl<NumericUpDown>("NUD_Player")!.Value = player;
        Click(w, "B_Save");

        Assert.Equal(player, museum.GetPlayers()[0]);
    }
}
