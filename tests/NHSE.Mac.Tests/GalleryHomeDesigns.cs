using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The friendly designs window (96-…): a capture of each kind of design list, and renaming, bringing a design
/// from a file and cancelling, checked in the save data after saving it to disk and reading it again.
/// </summary>
public class GalleryHomeDesigns
{
    private static HorizonSave Save(out string folder)
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        return TestSave.LoadCopy(out folder);
    }

    private static HorizonSave SaveAndReload(HorizonSave sav, string folder)
    {
        sav.Save((uint)DateTime.Now.Ticks);
        var reloaded = HorizonSave.FromFolder(folder);
        Assert.Empty(reloaded.GetInvalidHashes());
        return reloaded;
    }

    [AvaloniaFact]
    public void Captures()
    {
        var sav = Save(out _);
        var main = sav.Main;
        var me = sav.Players[0].Personal;
        Gallery.Capture(DesignsWindow.Normal(HomeStrings.Patterns, main.GetDesigns(), me, () => { }), "96-designs");
        Gallery.Capture(DesignsWindow.Pro(HomeStrings.ProDesigns, main.GetDesignsPRO(), me, () => { }), "96-designs-pro");
        Gallery.Capture(DesignsWindow.Normal(HomeStrings.FlagDesign, [main.FlagMyDesign], me, () => { }), "96-designs-flag");
        Gallery.Capture(DesignsWindow.Pro(HomeStrings.TailorDesigns, main.GetDesignsTailor(), me, () => { }), "96-designs-tailor");
    }

    [AvaloniaFact]
    public void RenameAndSave()
    {
        var sav = Save(out var folder);
        var main = sav.Main;
        var patterns = main.GetDesigns();
        var w = DesignsWindow.Normal(HomeStrings.Patterns, patterns, sav.Players[0].Personal, () => main.SetDesigns(patterns));
        w.Show();
        Dispatcher.UIThread.RunJobs();
        var index = w.Index;
        var name = w.Designs[index].Name;

        w.Designs[index].Name = "Prueba";
        w.SaveAndClose();
        Assert.Equal("Prueba", SaveAndReload(sav, folder).Main.GetDesign(index).DesignName);
        Assert.NotEqual("Prueba", name);
    }

    [AvaloniaFact]
    public async Task ImportIntoEmptySlot()
    {
        var sav = Save(out var folder);
        var main = sav.Main;
        var patterns = main.GetDesigns();
        // A design made by a player keeps its usage value (the game's samples get the one of player designs).
        var used = Array.FindIndex(patterns, z => new NormalDesign(z) is { IsEmpty: false, IsFromGame: false });
        var empty = Array.FindIndex(patterns, z => new NormalDesign(z).IsEmpty);
        Assert.SkipWhen(used < 0 || empty < 0, "the test save needs a used and an empty design slot");

        // Made by this player, so the window does not ask whether to make it theirs.
        var me = sav.Players[0].Personal;
        var design = new NormalDesign(new DesignPattern(patterns[used].Data.ToArray()));
        design.MakeMine(me);
        var file = Path.Combine(folder, "..", $"import-{Environment.ProcessId}.nhd");
        await File.WriteAllBytesAsync(file, design.Data.ToArray());
        try
        {
            var w = DesignsWindow.Normal(HomeStrings.Patterns, patterns, me, () => main.SetDesigns(patterns));
            w.Show();
            Dispatcher.UIThread.RunJobs();
            w.Select(empty);
            Assert.Equal(1, await w.Import([file]));
            Assert.False(w.Designs[empty].IsEmpty);
            Gallery.Capture(w, "96-designs-imported", false);
            w.SaveAndClose();
        }
        finally
        {
            File.Delete(file);
        }

        var reloaded = SaveAndReload(sav, folder).Main;
        Assert.Equal(design.Data.ToArray(), reloaded.GetDesign(empty).Data.ToArray());
    }

    [AvaloniaFact]
    public void OnlyDesignFilesAreRead()
    {
        var sav = Save(out var folder);
        var normal = Path.Combine(folder, "..", $"design-{Environment.ProcessId}.nhd");
        var pro = Path.Combine(folder, "..", $"design-{Environment.ProcessId}.nhpd");
        File.WriteAllBytes(normal, sav.Main.GetDesign(0).Data.ToArray());
        File.WriteAllBytes(pro, sav.Main.GetDesignPRO(0).Data.ToArray());
        try
        {
            Assert.True(DesignKind.Normal.CanRead(normal));
            Assert.False(DesignKind.Pro.CanRead(normal));
            Assert.True(DesignKind.Pro.CanRead(pro));
            Assert.False(DesignKind.Normal.CanRead(pro));
            Assert.False(DesignKind.Normal.CanRead(normal + ".missing"));
        }
        finally
        {
            File.Delete(normal);
            File.Delete(pro);
        }
    }

    [AvaloniaFact]
    public void CancelKeepsTheSave()
    {
        var sav = Save(out _);
        var main = sav.Main;
        var patterns = main.GetDesigns();
        var before = main.GetDesign(0).DesignName;
        var w = DesignsWindow.Normal(HomeStrings.Patterns, patterns, sav.Players[0].Personal, () => main.SetDesigns(patterns));
        w.Show();
        Dispatcher.UIThread.RunJobs();
        w.Designs[0].Name = "Otro";
        w.Close();
        Assert.Equal(before, main.GetDesign(0).DesignName);
    }
}
