using System.Diagnostics;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The core of NHSE for Switch (consola/source/core, C++), built here with clang: the save it writes must be sound for
/// NHSE and have exactly the changes NHSE makes.
/// </summary>
public class ConsolaTests
{
    private static readonly Lazy<string?> Tool = new(Build);

    private static string? Build()
    {
        var root = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "../../../../.."));
        var src = Path.Combine(root, "consola");
        if (!Directory.Exists(src))
            return null;
        var exe = Path.Combine(Path.GetTempPath(), "nhse-consola-prueba");
        var p = Process.Start(new ProcessStartInfo("clang++", ["-std=c++17", "-O2", "-Wall", "-o", exe, Path.Combine(src, "prueba/prueba.cpp"), Path.Combine(src, "source/core/acnh.cpp")]) { RedirectStandardError = true });
        if (p is null)
            return null;
        var err = p.StandardError.ReadToEnd();
        p.WaitForExit();
        Assert.True(p.ExitCode == 0, err);
        return exe;
    }

    private static string Run(string folder, params string[] args)
    {
        var p = Process.Start(new ProcessStartInfo(Tool.Value!, [folder, .. args]) { RedirectStandardOutput = true })!;
        var output = p.StandardOutput.ReadToEnd();
        p.WaitForExit();
        Assert.True(p.ExitCode == 0, output);
        return output;
    }

    private static void Skip()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        Assert.SkipWhen(Tool.Value is null, "no clang");
    }

    [Fact]
    public void ReadsTheSaveLikeNhse()
    {
        Skip();
        var sav = TestSave.LoadCopy(out var folder);
        var output = Run(folder, "estado");
        Assert.Contains("comprobaciones mal: main 0, personal 0", output);
        Assert.Contains($"monedero {sav.Players[0].Personal.Wallet.Value}", output);
        Assert.Contains($"banco {sav.Players[0].Personal.Bank.Value}", output);
    }

    [Fact]
    public void MoneyIsSavedSound()
    {
        Skip();
        TestSave.LoadCopy(out var folder);
        Run(folder, "dinero", "99999", "123456789", "54321");
        var sav = HorizonSave.FromFolder(folder);
        Assert.Empty(sav.GetInvalidHashes());
        var pers = sav.Players[0].Personal;
        Assert.Equal(99999u, pers.Wallet.Value);
        Assert.Equal(123456789u, pers.Bank.Value);
        Assert.Equal(54321u, pers.NookMiles.Value);
        // The checksums of the encrypted numbers are right too.
        EncryptedInt32.ReadVerify(pers.Data, pers.Offsets.Wallet);
        EncryptedInt32.ReadVerify(pers.Data, pers.Offsets.Bank);
    }

    /// <summary> What NHSE does with the same change, on the ground layer (as the map window does). </summary>
    private static byte[] NhseGround(HorizonSave sav, Action<LayerFieldItem> change)
    {
        var map = MapTileManagerUtil.FromSaveFile(sav.Main);
        change(map.FieldItems.Layer0);
        map.SetManager(sav.Main);
        return sav.Main.Data.Slice(sav.Main.Offsets.FieldItem, map.FieldItems.Layer0.Tiles.Length * Item.SIZE).ToArray();
    }

    private static bool IsFlower(Item t) => FieldItemList.Items.TryGetValue(t.ItemId, out var d) && d.Kind.IsFlower;

    [Fact]
    public void FlowersChangedLikeNhse()
    {
        Skip();
        var sav = TestSave.LoadCopy(out var folder);
        Run(folder, "flores", "EBED,EBE4", "alterna");
        var after = HorizonSave.FromFolder(folder);
        Assert.Empty(after.GetInvalidHashes());

        ushort[] ids = [0xEBED, 0xEBE4];
        var expected = NhseGround(sav, layer =>
        {
            var h = layer.TileInfo.TotalHeight;
            for (int i = 0; i < layer.Tiles.Length; i++)
            {
                var t = layer.Tiles[i];
                if (!t.IsRoot || !IsFlower(t))
                    continue;
                int x = i / h, y = i % h;
                GridItems.Remove(layer, x, y);
                GridItems.Place(layer, new Item(ids[((x / 2) + (y / 2)) % ids.Length]), x, y, out _);
            }
        });
        var actual = after.Main.Data.Slice(after.Main.Offsets.FieldItem, expected.Length).ToArray();
        Assert.Equal(expected, actual);
    }

    [Fact]
    public void WeedsRemovedLikeNhse()
    {
        Skip();
        var sav = TestSave.LoadCopy(out var folder);
        var output = Run(folder, "hierbas");
        Assert.Contains("quitadas 34", output);
        var after = HorizonSave.FromFolder(folder);
        Assert.Empty(after.GetInvalidHashes());
        var expected = NhseGround(sav, layer =>
        {
            var h = layer.TileInfo.TotalHeight;
            for (int i = 0; i < layer.Tiles.Length; i++)
            {
                var t = layer.Tiles[i];
                if (t.IsRoot && FieldItemList.Items.TryGetValue(t.ItemId, out var d) && d.Kind.IsWeed)
                    GridItems.Remove(layer, i / h, i % h);
            }
        });
        Assert.Equal(expected, after.Main.Data.Slice(after.Main.Offsets.FieldItem, expected.Length).ToArray());
    }
}
