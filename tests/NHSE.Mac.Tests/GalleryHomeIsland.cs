using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// Friendly island and museum windows of the home window: screenshots (galeria/9x-*.png) and what each one
/// writes to the save (through a copy of the test save, saved to disk and read again).
/// </summary>
public class GalleryHomeIsland
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

    /// <summary>
    /// Opens the window as a dialog, like the home window does, so the result of Save can be checked.
    /// </summary>
    private static Task<bool> ShowDialog(Window w)
    {
        var owner = new Window { Width = 200, Height = 200 };
        owner.Show();
        var result = w.ShowDialog<bool>(owner);
        Dispatcher.UIThread.RunJobs();
        return result;
    }

    private static void ClickSave(Window w)
    {
        var save = w.GetVisualDescendants().OfType<Button>().Single(z => z.IsDefault);
        save.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        Dispatcher.UIThread.RunJobs();
    }

    private static bool Saved(Task<bool> dialog)
    {
        Dispatcher.UIThread.RunJobs();
        Assert.True(dialog.IsCompletedSuccessfully);
        return dialog.Result;
    }

    [AvaloniaFact]
    public void Turnips()
    {
        var sav = Save(out var folder);
        var main = sav.Main;
        var before = main.Turnips;
        var t = main.Turnips;
        var w = new TurnipsWindow(t);
        var dialog = ShowDialog(w);
        Gallery.Capture(w, "90-turnips", false);

        w.PriceBox(DayOfWeek.Monday, false).Value = 123;
        w.PriceBox(DayOfWeek.Saturday, true).Value = 456;
        w.BuyBox.Value = 95;
        w.PatternButton(TurnipPattern.Decreasing).IsChecked = true;
        Assert.Equal(123u, t.SellMondayAM);
        Gallery.Capture(w, "90-turnips-edited", false);

        ClickSave(w);
        Assert.True(Saved(dialog));
        main.Turnips = t;

        var after = SaveAndReload(sav, folder).Main.Turnips;
        Assert.Equal(123u, after.SellMondayAM);
        Assert.Equal(456u, after.SellSaturdayPM);
        Assert.Equal(95u, after.BuyPrice);
        Assert.Equal(TurnipPattern.Decreasing, after.Pattern);
        Assert.Equal(before.SellFridayAM, after.SellFridayAM);
        Assert.Equal(before.SellSundayAM, after.SellSundayAM);
        Assert.Equal(before.FeverStart, after.FeverStart);
    }

    [AvaloniaFact]
    public void TurnipsCancelChangesNothing()
    {
        var sav = Save(out _);
        var t = sav.Main.Turnips;
        var w = new TurnipsWindow(t);
        var dialog = ShowDialog(w);
        w.PriceBox(DayOfWeek.Tuesday, false).Value = 600;
        w.Close(false);
        Assert.False(Saved(dialog));
        Assert.NotEqual(600u, sav.Main.Turnips.SellTuesdayAM);
    }

    [AvaloniaFact]
    public void Visitors()
    {
        var sav = Save(out var folder);
        var main = sav.Main;
        var v = main.Visitor;
        var tuesday = v.VisitorNPC[(int)DayOfWeek.Tuesday];
        var w = new VisitorsWindow(v);
        var dialog = ShowDialog(w);
        Gallery.Capture(w, "91-visitors", false);

        var monday = w.DayBox(DayOfWeek.Monday);
        monday.SelectedItem = monday.Items.OfType<VisitorsWindow.Choice>().Single(z => z.Value == VisitorNPC.Kicks);
        var thursday = w.DayBox(DayOfWeek.Thursday);
        thursday.SelectedItem = thursday.Items.OfType<VisitorsWindow.Choice>().Single(z => z.Value == VisitorNPC.None);
        Assert.Equal(VisitorNPC.Kicks, v.VisitorNPC[(int)DayOfWeek.Monday]);

        var expander = w.GetVisualDescendants().OfType<Expander>().Single();
        expander.IsExpanded = true;
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "91-visitors-advanced", false);

        ClickSave(w);
        Assert.True(Saved(dialog));
        main.Visitor = v;

        var after = SaveAndReload(sav, folder).Main.Visitor;
        Assert.Equal(VisitorNPC.Kicks, after.VisitorNPC[(int)DayOfWeek.Monday]);
        Assert.Equal(VisitorNPC.Kicks, after.InitSchedule[(int)DayOfWeek.Monday]);
        Assert.Equal(VisitorNPC.None, after.VisitorNPC[(int)DayOfWeek.Thursday]);
        Assert.Equal(tuesday, after.VisitorNPC[(int)DayOfWeek.Tuesday]);
        Assert.Equal(v.CreatePos.X, after.CreatePos.X);
    }

    [AvaloniaFact]
    public void Campsite()
    {
        var sav = Save(out var folder);
        var main = sav.Main;
        var w = new CampsiteWindow(main);
        var dialog = ShowDialog(w);
        Assert.True(w.Unlocked);
        Gallery.Capture(w, "92-campsite", false);

        w.SetCamper("cat01");
        Assert.Equal(DateTime.Today, w.VisitDate);
        Gallery.Capture(w, "92-campsite-camper", false);

        ClickSave(w);
        Assert.True(Saved(dialog));

        var after = SaveAndReload(sav, folder).Main;
        Assert.Equal(3, after.CampsiteStatus);
        Assert.Equal((byte)VillagerSpecies.cat, after.CampsiteVillagerSpeciesID);
        Assert.Equal(1, after.CampsiteVillagerVariantID);
        Assert.Equal(DateTime.Today, (DateTime)after.CampTimestamp);

        // And empty again
        var again = new CampsiteWindow(after);
        Assert.Equal("cat01", again.Camper);
        again.SetCamper(null);
        var dialog2 = ShowDialog(again);
        ClickSave(again);
        Assert.True(Saved(dialog2));
        Assert.Equal(2, after.CampsiteStatus);
        Assert.Equal((byte)VillagerSpecies.non, after.CampsiteVillagerSpeciesID);
    }

    [AvaloniaFact]
    public void CampsiteLocked()
    {
        var main = Save(out _).Main;
        main.CampsiteStatus = 1;
        var w = new CampsiteWindow(main);
        Assert.False(w.Unlocked);
        Gallery.Capture(w, "92-campsite-locked");
    }

    [AvaloniaFact]
    public void FruitsFlowers()
    {
        var sav = Save(out var folder);
        var main = sav.Main;
        var w = new FruitsFlowersWindow(sav.Players, 0, main);
        var dialog = ShowDialog(w);
        Assert.Equal(main.SpecialtyFruit, w.NativeFruit.Value);
        Gallery.Capture(w, "93-fruits", false);
        w.GetVisualDescendants().OfType<Expander>().Single().IsExpanded = true;
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "93-fruits-advanced", false);

        w.NativeFruit.Value = 2213; // apple
        w.SecondFruit.Value = 2287; // cherry
        w.SecondFlower.Value = IslandFlowers.Rose;
        ClickSave(w);
        Assert.True(Saved(dialog));

        var after = SaveAndReload(sav, folder);
        Assert.Equal(2213, after.Main.SpecialtyFruit);
        Assert.Equal(2287, after.Main.SisterFruit);
        Assert.Equal(2213, after.Players[0].Personal.ProfileFruit);
        Assert.Equal(new byte[] { 1, 0, 0, 0, 0 }, after.Main.FruitFlags.ToArray());
        Assert.Equal(IslandFlowers.Rose, after.Main.SisterFlower);
        Assert.Equal(main.SpecialtyFlower, after.Main.SpecialtyFlower);
    }

    [AvaloniaFact]
    public void Museum()
    {
        var sav = Save(out var folder);
        var main = sav.Main;
        var museum = main.Museum;
        var entriesBefore = museum.GetItems().Count(z => !z.IsNone);
        var w = new MuseumWindow(museum, sav.Players, 0);
        var dialog = ShowDialog(w);

        // What the museum of the game has: 80 fish, 80 bugs, 40 sea creatures, 73 fossils and 43 works of art.
        Assert.Equal(80, w.Pieces(MuseumWindow.Wing.Fish).Length);
        Assert.Equal(80, w.Pieces(MuseumWindow.Wing.Bugs).Length);
        Assert.Equal(40, w.Pieces(MuseumWindow.Wing.Sea).Length);
        Assert.Equal(73, w.Pieces(MuseumWindow.Wing.Fossils).Length);
        Assert.Equal(43, w.Pieces(MuseumWindow.Wing.Art).Length);

        foreach (var wing in Enum.GetValues<MuseumWindow.Wing>())
        {
            w.SelectWing(wing);
            Dispatcher.UIThread.RunJobs();
            Gallery.Capture(w, $"94-museum-{(int)wing}", false);
        }

        // Donate every fish, then take out one that was there before
        var fishBefore = w.Progress(MuseumWindow.Wing.Fish).Donated;
        w.DonateAll(MuseumWindow.Wing.Fish);
        Assert.Equal((80, 80), w.Progress(MuseumWindow.Wing.Fish));
        var firstEntry = museum.GetItems()[0].ItemId;
        var removed = w.Pieces(MuseumWindow.Wing.Fish).First(z => z.Id != firstEntry && museum.GetItems().Any(i => i.ItemId == z.Id));
        w.SetDonated(removed, false);
        Assert.False(w.IsDonated(removed.Id));
        Assert.Equal((79, 80), w.Progress(MuseumWindow.Wing.Fish));
        w.SelectWing(MuseumWindow.Wing.Fish);
        Gallery.Capture(w, "94-museum-donated", false);

        ClickSave(w);
        Assert.True(Saved(dialog));
        main.Museum = museum;

        var after = SaveAndReload(sav, folder).Main.Museum;
        var items = after.GetItems();
        var dates = after.GetDates();
        var players = after.GetPlayers();
        var count = items.Count(z => !z.IsNone);
        Assert.Equal(entriesBefore + (80 - fishBefore) - 1, count);
        // Still a list without gaps, in donation order, and the empty end has no player
        Assert.All(items.Take(count), z => Assert.False(z.IsNone));
        Assert.All(items.Skip(count), z => Assert.True(z.IsNone));
        Assert.All(players.Skip(count), z => Assert.Equal(byte.MaxValue, z));
        Assert.Equal(firstEntry, items[0].ItemId);
        Assert.DoesNotContain(items, z => z.ItemId == removed.Id);
        var fish = w.Pieces(MuseumWindow.Wing.Fish).Select(z => z.Id).ToHashSet();
        Assert.Equal(79, items.Count(z => fish.Contains(z.ItemId)));
        // The new ones are from today, by the first player
        Assert.Equal(DateTime.Today, (DateTime)dates[count - 1]);
        Assert.Equal(0, players[count - 1]);
    }

    [AvaloniaFact]
    public void IslandAndMuseumPages()
    {
        var w = new HomeWindow(Save(out _));
        w.Show();
        var nav = w.FindControl<ListBox>("LB_Nav")!;
        nav.SelectedIndex = 2;
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "95-home-island", false);
        nav.SelectedIndex = 3;
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "95-home-museum");
    }
}
