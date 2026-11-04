using System.Runtime.InteropServices;
using Avalonia.Headless.XUnit;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// «Avanzado» of NHSE for Switch: each change leaves the save byte for byte as NHSE (and the Mac app) leaves it.
/// </summary>
public class ConsolaAvanzadoTests
{
    private static string Folder() => Path.Combine(TestSave.SavesFolder, "avanzado-" + Guid.NewGuid().ToString("N"));

    /// <summary> The first known flag with this value (and the kind asked for: yes/no or counter). </summary>
    private static ushort FirstFlag(IEnumerable<(ushort Index, short Max)> known, short[] values, bool yesNo, short value) =>
        known.Where(z => z.Index < values.Length && (z.Max == 1) == yesNo && values[z.Index] == value).OrderBy(z => z.Index).First().Index;

    [AvaloniaFact]
    public void IslandFlagLikeTheMac()
    {
        ConsolaTests.Skip();
        var main = TestSave.LoadCopy(out _).Main;
        var values = main.EventFlagLand.ToArray();
        var index = FirstFlag(EventFlagLand.List.Select(z => (z.Key, z.Value.MaxValue)), values, true, 0);
        ConsolaTests.Both(sav =>
        {
            var edit = sav.Main.EventFlagLand.ToArray();
            var w = new EventFlagsWindow(edit, true, "isla");
            w.SetValue(index, 1);
            edit.CopyTo(sav.Main.EventFlagLand);
        }, "marca", "isla", index.ToString(), "1");
    }

    [AvaloniaFact]
    public void PlayerFlagLikeTheMac()
    {
        ConsolaTests.Skip();
        var pers = TestSave.LoadCopy(out _).Players[0].Personal;
        var values = pers.GetEventFlagsPlayer();
        var index = FirstFlag(EventFlagPlayer.List.Select(z => (z.Key, z.Value.MaxValue)), values, false, 0);
        ConsolaTests.Both(sav =>
        {
            var p = sav.Players[0].Personal;
            var flags = p.GetEventFlagsPlayer();
            var w = new EventFlagsWindow(flags, false, "jugador");
            w.SetValue(index, 7);
            p.SetEventFlagsPlayer(flags);
        }, "marca", "jugador", index.ToString(), "7");
    }

    [Fact]
    public void FlagsExportedLikeNhse()
    {
        ConsolaTests.Skip();
        var sav = TestSave.LoadCopy(out var folder);
        Directory.CreateDirectory(TestSave.SavesFolder);
        var land = Folder() + ".nhfl";
        var player = Folder() + ".nhfl";
        ConsolaTests.Run(folder, "marcas-exportar", "isla", land);
        ConsolaTests.Run(folder, "marcas-exportar", "jugador", player);
        // As EventFlagsWindow.Export writes them.
        Assert.Equal(MemoryMarshal.AsBytes(sav.Main.EventFlagLand).ToArray(), File.ReadAllBytes(land));
        Assert.Equal(MemoryMarshal.AsBytes(sav.Players[0].Personal.GetEventFlagsPlayer().AsSpan()).ToArray(), File.ReadAllBytes(player));
    }

    [Fact]
    public void FlagsImportedLikeNhse()
    {
        ConsolaTests.Skip();
        // A flag file with the island flags all set to 1.
        Directory.CreateDirectory(TestSave.SavesFolder);
        var file = Folder() + ".nhfl";
        var data = new byte[0x400 * 2];
        for (int i = 0; i < data.Length; i += 2)
            data[i] = 1;
        File.WriteAllBytes(file, data);
        ConsolaTests.Both(sav =>
        {
            var values = sav.Main.EventFlagLand.ToArray();
            Buffer.BlockCopy(data, 0, values, 0, data.Length);
            values.CopyTo(sav.Main.EventFlagLand);
        }, "marcas-cargar", "isla", file);
    }

    [Fact]
    public void WeatherLikeTheMac()
    {
        ConsolaTests.Both(sav =>
        {
            sav.Main.WeatherSeed = 123456789;
            sav.Main.TourWeatherSeed = 4000000000;
            sav.Main.TourHemisphere = Hemisphere.Southern;
        }, "clima", "123456789", "4000000000", "1");
    }

    [Fact]
    public void OpenSlotsLikeTheClassicEditor()
    {
        ConsolaTests.Both(sav =>
        {
            var p = sav.Players[0].Personal;
            p.PocketCount = 10;
            p.BagCount = 20;
            p.ItemChestCount = 5000;
        }, "huecos", "10", "20", "5000");
    }

    [Fact]
    public void RawBytesLikeNhse()
    {
        ConsolaTests.Both(sav =>
        {
            byte[] b = [0x12, 0xAB, 0x00, 0xFF];
            b.CopyTo(sav.Main.Data[0x22F000..]);
        }, "bytes", "main", "0x22F000", "12 ab 00ff");
        ConsolaTests.Both(sav => sav.Players[0].Personal.Data[0x37D60] = 0x5A, "bytes", "0", "37D60", "5A");
    }

    [Fact]
    public void ChecksLikeNhse()
    {
        ConsolaTests.Skip();
        var sav = TestSave.LoadCopy(out var folder);
        var output = ConsolaTests.Run(folder, "comprobar", "romper");
        sav.Players[0].PostBox.Data[0x200] ^= 1;
        var bad = sav.GetInvalidHashes().Select(z => z.ToString()).ToArray();
        Assert.Single(bad);
        Assert.Contains("Villager0/postbox.dat: 1 de 1 mal " + bad[0], output);
        Assert.Contains("main.dat: 0 de 19 mal", output);
        foreach (var p in sav.Players)
        {
            foreach (var pair in p)
                Assert.Contains($"{p.DirectoryName}/{pair.NameData}: ", output);
        }
    }

    [Fact]
    public void ExportedLikeNhse()
    {
        ConsolaTests.Skip();
        var sav = TestSave.LoadCopy(out var folder);
        var mine = Folder();
        var nhse = Folder();
        ConsolaTests.Run(folder, "exportar", mine);
        sav.Dump(nhse);
        var files = Directory.GetFiles(nhse, "*", SearchOption.AllDirectories).Select(z => Path.GetRelativePath(nhse, z)).Order().ToArray();
        Assert.Equal(files, Directory.GetFiles(mine, "*", SearchOption.AllDirectories).Select(z => Path.GetRelativePath(mine, z)).Order().ToArray());
        foreach (var f in files)
            ConsolaTests.SameBytes(File.ReadAllBytes(Path.Combine(nhse, f)), File.ReadAllBytes(Path.Combine(mine, f)));
    }

    [Fact]
    public void ImportedLikeNhse()
    {
        ConsolaTests.Skip();
        // A decrypted copy with changes (and its checksums not updated, as NHSE exports a save being edited).
        var source = TestSave.LoadCopy(out _);
        source.Main.WeatherSeed = 42;
        source.Players[0].Personal.Data[0x37D60] ^= 0x55;
        var dir = Folder();
        source.Dump(dir);
        ConsolaTests.Both(sav => sav.Load(dir), "cargar", dir);
    }
}
