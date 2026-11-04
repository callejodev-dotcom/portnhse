using System.Diagnostics;
using Avalonia.Headless.XUnit;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The core of NHSE for Switch (consola/source/core, C++), built here with clang: the save it writes must be sound for
/// NHSE and have exactly the changes NHSE makes.
/// </summary>
public partial class ConsolaTests
{
    private static readonly Lazy<string?> Tool = new(Build);

    private static string? Build()
    {
        var root = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "../../../../.."));
        var src = Path.Combine(root, "consola");
        if (!Directory.Exists(src))
            return null;
        // One per copy of the repo (worktrees build at the same time).
        var exe = Path.Combine(Path.GetTempPath(), $"nhse-consola-prueba-{(uint)root.GetHashCode():X8}");
        string[] sources = [.. Directory.GetFiles(Path.Combine(src, "prueba"), "*.cpp"), .. Directory.GetFiles(Path.Combine(src, "source/core"), "*.cpp")];
        var p = Process.Start(new ProcessStartInfo("clang++", ["-std=c++17", "-O2", "-Wall", "-o", exe, .. sources, "-lz"]) { RedirectStandardError = true });
        if (p is null)
            return null;
        var err = p.StandardError.ReadToEnd();
        p.WaitForExit();
        Assert.True(p.ExitCode == 0, err);
        return exe;
    }

    internal static string Run(string folder, params string[] args)
    {
        var p = Process.Start(new ProcessStartInfo(Tool.Value!, [folder, .. args]) { RedirectStandardOutput = true })!;
        var output = p.StandardOutput.ReadToEnd();
        p.WaitForExit();
        Assert.True(p.ExitCode == 0, output);
        return output;
    }

    internal static void Skip()
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

    /// <summary> A villager changed for another one, in the same house, exactly as NHSE does it (VillagerSwap). </summary>
    [Fact]
    public void VillagerSwappedLikeNhse()
    {
        Skip();
        var sav = TestSave.LoadCopy(out var folder);
        var bin = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "../../../../../consola/romfs/vecinos.bin"));
        var output = Run(folder, "vecino", "1", "wol08", bin);
        Assert.Contains("ahora: wol08", output);
        var after = HorizonSave.FromFolder(folder);
        Assert.Empty(after.GetInvalidHashes());

        var villagers = sav.Main.GetVillagers();
        var v2 = (Villager2)villagers[1];
        var houses = sav.Main.GetVillagerHouses();
        var hi = Array.FindIndex(houses, z => z.NPC1 == 1);
        var rep = NHSE.Villagers.VillagerSwap.GetReplacementVillager(new NHSE.Villagers.VillagerInfo(v2, houses[hi]), "wol08");
        sav.Main.SetVillagerHouse(new VillagerHouse1(rep.House), hi);
        villagers[1] = new Villager2(rep.Villager);
        sav.Main.SetVillagers(villagers);
        sav.Main.Hash();
        var expected = sav.Main.Data.ToArray();
        var actual = after.Main.Data.ToArray();
        var first = Enumerable.Range(0, expected.Length).FirstOrDefault(i => expected[i] != actual[i], -1);
        Assert.True(first < 0, $"first difference at 0x{first:X}");
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
    /// <summary> The whole file the Switch wrote is the one NHSE writes with the same change. </summary>
    internal static void SameBytes(ReadOnlySpan<byte> expected, ReadOnlySpan<byte> actual)
    {
        Assert.Equal(expected.Length, actual.Length);
        var first = -1;
        for (int i = 0; i < expected.Length && first < 0; i++)
        {
            if (expected[i] != actual[i])
                first = i;
        }
        Assert.True(first < 0, $"first difference at 0x{first:X}");
    }

    /// <summary> Runs a change on the Switch core and the same change in NHSE, and compares both saves. </summary>
    internal static (HorizonSave Nhse, HorizonSave Switch) Both(Action<HorizonSave> nhse, params string[] args)
    {
        Skip();
        var sav = TestSave.LoadCopy(out var folder);
        Run(folder, args);
        var after = HorizonSave.FromFolder(folder);
        Assert.Empty(after.GetInvalidHashes());
        nhse(sav);
        sav.Main.Hash();
        sav.Players[0].Personal.Hash();
        SameBytes(sav.Main.Data, after.Main.Data);
        SameBytes(sav.Players[0].Personal.Data, after.Players[0].Personal.Data);
        return (sav, after);
    }

    [Fact]
    public void PocketItemLikeNhse()
    {
        // Slot 4 (first part of the pockets) gets 30 iron nuggets.
        var (_, after) = Both(sav =>
        {
            var pers = sav.Players[0].Personal;
            var pocket = pers.Pocket.ToArray();
            pocket[3] = new Item(2511) { Count = 29 };
            pers.Pocket = pocket;
        }, "bolsillo", "3", "2511", "29");
        Assert.Equal(2511, after.Players[0].Personal.Pocket[3].ItemId);
    }

    [Fact]
    public void PocketColorAndWrapLikeNhse()
    {
        // Slot 2 (the net): colour 3 and wrapped in blue paper (9), as the item editor of the Mac does it.
        Both(sav =>
        {
            var pers = sav.Players[0].Personal;
            var pocket = pers.Pocket.ToArray();
            var item = new Item(pocket[2].ItemId) { BodyType = 3 };
            item.SetWrapping(ItemWrapping.WrappingPaper, ItemWrappingPaper.Blue, true);
            pocket[2] = item;
            pers.Pocket = pocket;
        }, "bolsillo", "2", Pocket2Id().ToString(), "3", "0x65");
    }

    private static ushort Pocket2Id() => TestSave.LoadCopy(out _).Players[0].Personal.Pocket[2].ItemId;

    [Fact]
    public void PocketRecipeLikeNhse()
    {
        // Slot 26 (second part) gets the DIY recipe card of the ladder (recipe 107).
        Both(sav =>
        {
            var pers = sav.Players[0].Personal;
            var bag = pers.Bag.ToArray();
            bag[5] = new Item(Item.DIYRecipe) { Count = 107 };
            pers.Bag = bag;
        }, "bolsillo", "25", "0x16A2", "107");
    }

    [AvaloniaFact]
    public void RecipesLearnedLikeNhse()
    {
        Both(sav =>
        {
            var w = new RecipesWindow(sav.Players[0].Personal);
            w.LearnAll();
            w.SaveAndClose();
        }, "recetas");
    }

    [AvaloniaFact]
    public void CatalogCompletedLikeNhse()
    {
        Both(sav =>
        {
            var w = new CatalogWindow(sav.Players[0]);
            w.ShowCategory(ItemCategory.All);
            w.MarkShown(true);
            w.SaveAndClose();
        }, "catalogo");
    }

    [AvaloniaFact]
    public void ReactionsLearnedLikeNhse()
    {
        Both(sav =>
        {
            var w = new ReactionsWindow(sav.Players[0].Personal);
            w.LearnAll();
            w.SaveAndClose();
        }, "reacciones");
    }

    [Fact]
    public void BestFriendsLikeNhse()
    {
        // As «Best friends with everyone» of the friendship window: every player the villager remembers.
        Both(sav =>
        {
            var villagers = sav.Main.GetVillagers();
            foreach (var v in villagers.OfType<Villager2>())
            {
                foreach (var m in v.GetMemories().Where(m => !string.IsNullOrWhiteSpace(m.PlayerName)))
                    m.Friendship = byte.MaxValue;
            }
            sav.Main.SetVillagers(villagers);
        }, "amigos");
    }

    [Fact]
    public void TurnipsLikeNhse()
    {
        Both(sav =>
        {
            var t = sav.Main.Turnips;
            t.BuyPrice = 95;
            for (int half = 2; half < 14; half++)
                TurnipsWindow.SetPrice(t, half, 600);
            sav.Main.Turnips = t;
        }, "nabos", "95", "600");
    }

    [Fact]
    public void ItemSearchLikeNhse()
    {
        Skip();
        TestSave.LoadCopy(out var folder);
        // Without case or accents, every word: «ARBOL» finds «árbol», «pepita oro» finds «pepita de oro».
        Assert.Contains("  pepita de oro\n", Run(folder, "buscar", "PEPITA oro"));
        var output = Run(folder, "buscar", "arbol");
        Assert.Contains("árbol", output);
    }
}
