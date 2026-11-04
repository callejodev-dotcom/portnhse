using System.IO.Compression;
using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The designs of NHSE for Switch (consola/source/core/disenos.cpp): every change leaves the save byte for byte as the
/// designs window of the Mac app leaves it (renaming, making it yours, bringing files in, copying, emptying), and the
/// files and pictures it writes are the ones of the Mac.
/// </summary>
public class ConsolaDisenosTests
{
    private static readonly string Files = Path.Combine(TestSave.SavesFolder, "disenos-" + Guid.NewGuid().ToString("N"));

    /// <summary> The designs window of the Mac for that kind, writing back to <paramref name="sav"/> when saved. </summary>
    private static DesignsWindow Window(HorizonSave sav, string kind)
    {
        var main = sav.Main;
        var me = sav.Players[0].Personal;
        DesignsWindow w;
        switch (kind)
        {
            case "pro":
            {
                var p = main.GetDesignsPRO();
                w = DesignsWindow.Pro(HomeStrings.ProDesigns, p, me, () => main.SetDesignsPRO(p));
                break;
            }
            case "bandera":
            {
                var p = new[] { main.FlagMyDesign };
                w = DesignsWindow.Normal(HomeStrings.FlagDesign, p, me, () => main.FlagMyDesign = p[0]);
                break;
            }
            case "sastreria":
            {
                var p = main.GetDesignsTailor();
                w = DesignsWindow.Pro(HomeStrings.TailorDesigns, p, me, () => main.SetDesignsTailor(p));
                break;
            }
            default:
            {
                var p = main.GetDesigns();
                w = DesignsWindow.Normal(HomeStrings.Patterns, p, me, () => main.SetDesigns(p));
                break;
            }
        }
        w.Show();
        Dispatcher.UIThread.RunJobs();
        return w;
    }

    /// <summary> Import of the window (it awaits reading the files): runs the UI jobs until it ends. </summary>
    private static int Import(DesignsWindow w, int slot, params string[] files)
    {
        w.Select(slot);
        var task = w.Import(files);
        while (!task.IsCompleted)
            Dispatcher.UIThread.RunJobs();
        return task.Result;
    }

    private static string File(string name, byte[] data)
    {
        Directory.CreateDirectory(Files);
        var path = Path.Combine(Files, name);
        System.IO.File.WriteAllBytes(path, data);
        return path;
    }

    private static DesignSlot[] Slots(HorizonSave sav, string kind) => kind switch
    {
        "pro" => [.. sav.Main.GetDesignsPRO().Select(z => new ProDesign(z))],
        "bandera" => [new NormalDesign(sav.Main.FlagMyDesign)],
        "sastreria" => [.. sav.Main.GetDesignsTailor().Select(z => new ProDesign(z))],
        _ => [.. sav.Main.GetDesigns().Select(z => new NormalDesign(z))],
    };

    [Fact]
    public void ReadsTheDesignsLikeNhse()
    {
        ConsolaTests.Skip();
        var sav = TestSave.LoadCopy(out var folder);
        foreach (var kind in new[] { "normal", "pro", "bandera", "sastreria" })
        {
            var lines = ConsolaTests.Run(folder, "disenos", kind).Split('\n').Where(z => z.Contains(": «")).ToArray();
            var slots = Slots(sav, kind);
            Assert.Equal(slots.Length, lines.Length);
            for (int i = 0; i < slots.Length; i++)
            {
                var d = slots[i];
                Assert.StartsWith($"{i}: «{d.Name}» de {d.Maker} ({d.Town})", lines[i]);
                Assert.Equal(d.IsEmpty, lines[i].Contains(" vacio"));
                Assert.Equal(d.IsFromGame, lines[i].Contains(" juego"));
                Assert.Equal(!d.IsFromGame && d.IsMadeBy(sav.Players[0].Personal), lines[i].Contains(" mio"));
            }
        }
    }

    [AvaloniaTheory]
    [InlineData("normal", 10, "Estrellas rosas")]
    [InlineData("normal", 3, "Un nombre larguísimo de más de veinte letras")]
    [InlineData("normal", 12, "")]
    [InlineData("pro", 2, "Camisa de Carla ñ")]
    [InlineData("bandera", 0, "Bandera nueva")]
    [InlineData("sastreria", 5, "Gorra")]
    public void RenameLikeNhse(string kind, int slot, string name)
    {
        ConsolaTests.Both(sav =>
        {
            var w = Window(sav, kind);
            w.Designs[slot].Name = name;
            w.SaveAndClose();
        }, "diseno-nombre", kind, slot.ToString(), name);
    }

    [AvaloniaTheory]
    [InlineData("normal", 0)]
    [InlineData("pro", 4)]
    [InlineData("sastreria", 1)]
    public void MakeMineLikeNhse(string kind, int slot)
    {
        // The game's samples are not made by the player: «make it mine» changes their island and player.
        var (_, after) = ConsolaTests.Both(sav =>
        {
            var w = Window(sav, kind);
            w.Designs[slot].MakeMine(sav.Players[0].Personal);
            w.SaveAndClose();
        }, "diseno-mio", kind, slot.ToString());
        Assert.True(Slots(after, kind)[slot].IsMadeBy(after.Players[0].Personal));
    }

    [AvaloniaFact]
    public void ImportLikeNhse()
    {
        // One made by the player (keeps its usage) and two samples of the game (their usage is fixed), into the
        // chosen slot and the ones after it, as dragging three files into the window.
        ConsolaTests.Skip();
        var src = TestSave.LoadCopy(out var folder);
        var mine = File("mio.nhd", src.Main.GetDesign(10).Data.ToArray());
        var notes = File("notas.nhd", src.Main.GetDesign(4).Data.ToArray());
        var plane = File("avion.nhd", src.Main.GetDesign(0).Data.ToArray());

        // The file the Switch writes is the design as it is in the save (and as the Mac writes it).
        var exported = Path.Combine(Files, "exportado.nhd");
        Assert.Contains("archivo 15 - star.nhd", ConsolaTests.Run(folder, "diseno-exportar", "normal", "14", exported));
        Assert.Equal(src.Main.GetDesign(14).Data.ToArray(), System.IO.File.ReadAllBytes(exported));

        ConsolaTests.Both(sav =>
        {
            var w = Window(sav, "normal");
            // One by one: with several files the window says how many it loaded in a message box.
            Assert.Equal(1, Import(w, 40, mine));
            Assert.Equal(1, Import(w, 41, notes));
            Assert.Equal(1, Import(w, 42, plane));
            w.SaveAndClose();
        }, "diseno-importar", "normal", "40", "si", mine, notes, plane);
    }

    [AvaloniaFact]
    public void ImportSomeoneElsesLikeNhse()
    {
        // A design made by someone else: «yes, make it mine» (the Mac asks; here it is made the player's first, so the
        // window does not ask and does the same).
        ConsolaTests.Skip();
        var src = TestSave.LoadCopy(out _);
        var data = src.Main.GetDesign(10).Data.ToArray();
        var other = new DesignPattern(data) { PlayerID = 0x12345678, PlayerName = "Lola", TownID = 0x9ABCDEF0, TownName = "Otra" };
        var file = File("lola.nhd", other.Data.ToArray());
        var made = new NormalDesign(new DesignPattern(other.Data.ToArray()));
        Assert.False(made.IsMadeBy(src.Players[0].Personal));
        made.MakeMine(src.Players[0].Personal);
        var mineFile = File("lola-mio.nhd", made.Data.ToArray());

        var (_, after) = ConsolaTests.Both(sav =>
        {
            var w = Window(sav, "normal");
            Assert.Equal(1, Import(w, 50, mineFile));
            w.SaveAndClose();
        }, "diseno-importar", "normal", "50", "si", file);
        Assert.True(new NormalDesign(after.Main.GetDesign(50)).IsMadeBy(after.Players[0].Personal));

        // «No, leave it as it is»: only the usage is fixed, as Import does.
        ConsolaTests.Both(sav =>
        {
            var patterns = sav.Main.GetDesigns();
            var d = new NormalDesign(new DesignPattern(other.Data.ToArray()));
            d.FixUsage();
            patterns[50] = d.Pattern;
            sav.Main.SetDesigns(patterns);
        }, "diseno-importar", "normal", "50", "no", file);
    }

    [AvaloniaTheory]
    [InlineData("pro", 20, 3)]
    [InlineData("sastreria", 2, 9)]
    [InlineData("bandera", 0, 13)]
    public void ImportOtherKindsLikeNhse(string kind, int slot, int from)
    {
        ConsolaTests.Skip();
        var src = TestSave.LoadCopy(out _);
        var file = kind == "bandera"
            ? File($"b{from}.nhd", src.Main.GetDesign(from).Data.ToArray())
            : File($"p{kind}{from}.nhpd", src.Main.GetDesignPRO(from).Data.ToArray());
        ConsolaTests.Both(sav =>
        {
            var w = Window(sav, kind);
            Assert.Equal(1, Import(w, slot, file));
            w.SaveAndClose();
        }, "diseno-importar", kind, slot.ToString(), "si", file);
    }

    [AvaloniaTheory]
    [InlineData("normal", 4, 60)]
    [InlineData("normal", 10, 2)]
    [InlineData("pro", 1, 30)]
    [InlineData("sastreria", 0, 7)]
    public void CopyLikeNhse(string kind, int from, int to)
    {
        // Copying to another slot is saving it to a file and bringing it into the other slot.
        ConsolaTests.Skip();
        var src = TestSave.LoadCopy(out _);
        var file = File($"copia-{kind}-{from}{(kind == "normal" ? ".nhd" : ".nhpd")}", Slots(src, kind)[from].Data.ToArray());
        ConsolaTests.Both(sav =>
        {
            var w = Window(sav, kind);
            Assert.Equal(1, Import(w, to, file));
            w.SaveAndClose();
        }, "diseno-copiar", kind, from.ToString(), to.ToString());
    }

    [AvaloniaTheory]
    [InlineData("normal", 10)]
    [InlineData("pro", 0)]
    [InlineData("bandera", 0)]
    [InlineData("sastreria", 3)]
    public void EmptyLikeNhse(string kind, int slot)
    {
        // Emptying puts the game's unused slot (the first empty one of its size) in it.
        var (_, after) = ConsolaTests.Both(sav =>
        {
            var blanks = Slots(sav, kind is "pro" or "sastreria" ? "pro" : "normal");
            var blank = blanks.First(z => z.IsEmpty).Data.ToArray();
            var w = Window(sav, kind);
            blank.CopyTo(w.Designs[slot].Data);
            w.SaveAndClose();
        }, "diseno-vaciar", kind, slot.ToString());
        Assert.True(Slots(after, kind)[slot].IsEmpty);
    }

    [AvaloniaTheory]
    [InlineData("normal", 4)]
    [InlineData("normal", 14)]
    [InlineData("pro", 0)]
    [InlineData("sastreria", 6)]
    public void PictureLikeNhse(string kind, int slot)
    {
        // «Save as picture»: the same pixels as the Mac's PNG (8 times for normal designs, 4 for PRO ones).
        ConsolaTests.Skip();
        var sav = TestSave.LoadCopy(out var folder);
        Directory.CreateDirectory(Files);
        var png = Path.Combine(Files, $"{kind}{slot}.png");
        ConsolaTests.Run(folder, "diseno-png", kind, slot.ToString(), png);
        var d = Slots(sav, kind)[slot];
        using var expected = d.GetPicture(d.SheetCount == 1 ? 8 : 4);
        var (w, h, argb) = ReadPng(System.IO.File.ReadAllBytes(png));
        Assert.Equal(expected.PixelSize.Width, w);
        Assert.Equal(expected.PixelSize.Height, h);
        Assert.Equal(expected.GetPixels(), argb);
    }

    /// <summary> A PNG as the Switch writes it (RGBA, 8 bits, no filters): its pixels as ARGB. </summary>
    private static (int W, int H, int[] Argb) ReadPng(byte[] png)
    {
        int Be(int o) => (png[o] << 24) | (png[o + 1] << 16) | (png[o + 2] << 8) | png[o + 3];
        int w = 0, h = 0;
        using var idat = new MemoryStream();
        for (int o = 8; o < png.Length;)
        {
            var len = Be(o);
            var type = System.Text.Encoding.ASCII.GetString(png, o + 4, 4);
            if (type == "IHDR")
            {
                (w, h) = (Be(o + 8), Be(o + 12));
                Assert.Equal(8, png[o + 16]);
                Assert.Equal(6, png[o + 17]);
            }
            else if (type == "IDAT")
            {
                idat.Write(png, o + 8, len);
            }
            o += 12 + len;
        }
        idat.Position = 0;
        using var z = new ZLibStream(idat, CompressionMode.Decompress);
        using var raw = new MemoryStream();
        z.CopyTo(raw);
        var bytes = raw.ToArray();
        var argb = new int[w * h];
        for (int y = 0; y < h; y++)
        {
            var row = y * ((w * 4) + 1);
            Assert.Equal(0, bytes[row]);
            for (int x = 0; x < w; x++)
            {
                var p = row + 1 + (x * 4);
                argb[(y * w) + x] = (bytes[p + 3] << 24) | (bytes[p] << 16) | (bytes[p + 1] << 8) | bytes[p + 2];
            }
        }
        return (w, h, argb);
    }
}
